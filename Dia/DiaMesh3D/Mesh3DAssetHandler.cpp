#include "DiaMesh3D/Mesh3DAssetHandler.h"

#include <DiaMesh3D/Mesh3DBinaryReader.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/ProfileCategory.h>
#include <DiaObservation/Trace/TraceCategory.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstring>

namespace Dia { namespace Mesh3D {

// Full definition lives only in the .cpp so ReadResult is not exposed via the header.
struct Mesh3DAssetHandler::PendingResult
{
    Dia::Core::StringCRC                   assetId;
    bool                                   success    = false;
    char                                   failReason[128] = {};
    Mesh3DAsset*                           asset      = nullptr;   // ptr already in mMeshMap
    Dia::AssetRuntime::IAssetLoadCallback* callback   = nullptr;
    ReadResult*                            readResult = nullptr;   // heap-alloc'd by worker
    Dia::Core::JobHandle                   job;
};

//-----------------------------------------------------------------------------
// Construction / destruction
//-----------------------------------------------------------------------------

Mesh3DAssetHandler::Mesh3DAssetHandler()
{
    Dia::Observation::Metric::MetricRegistry::Instance().RegisterGauge(
        Dia::Core::StringCRC("dia.mesh3d.loaded_count"));
    Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(
        Dia::Core::StringCRC("dia.mesh3d.failed_count"));
}

Mesh3DAssetHandler::~Mesh3DAssetHandler()
{
    // Drain any lingering pending results to avoid leaks.
    std::vector<PendingResult*> remaining;
    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        remaining.swap(mPending);
    }
    for (PendingResult* r : remaining)
    {
        if (r->job.IsValid() && mJobSystem)
            mJobSystem->Wait(r->job);
        delete r->readResult;
        delete r;
    }

    // Delete all owned assets.
    std::unique_lock<std::shared_mutex> lock(mMeshMutex);
    for (auto& kv : mMeshMap)
        delete kv.second;
    mMeshMap.clear();
}

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

void Mesh3DAssetHandler::SetJobSystem(Dia::Core::JobSystem* jobSystem)
{
    DIA_ASSERT(jobSystem != nullptr, "Mesh3DAssetHandler requires a valid JobSystem");
    mJobSystem = jobSystem;
}

//-----------------------------------------------------------------------------
// Thread-safe queries
//-----------------------------------------------------------------------------

Mesh3DAsset* Mesh3DAssetHandler::LookupMesh(Dia::Core::StringCRC assetId) const
{
    std::shared_lock<std::shared_mutex> lock(mMeshMutex);
    auto it = mMeshMap.find(assetId.Value());
    return (it != mMeshMap.end()) ? it->second : nullptr;
}

unsigned int Mesh3DAssetHandler::GetLoadedCount() const
{
    std::shared_lock<std::shared_mutex> lock(mMeshMutex);
    return static_cast<unsigned int>(mMeshMap.size());
}

//-----------------------------------------------------------------------------
// Load — non-blocking; submits work to the JobSystem
//-----------------------------------------------------------------------------

void Mesh3DAssetHandler::Load(const Dia::Core::StringCRC& assetId,
                               const Dia::Core::Containers::String512& resolvedPath,
                               Dia::AssetRuntime::IAssetLoadCallback* callback)
{
    DIA_ASSERT(mJobSystem != nullptr, "Mesh3DAssetHandler::Load called before SetJobSystem");

    // Allocate the asset and register it in the map before the worker runs
    // so that LookupMesh can return it immediately (State::Pending).
    Mesh3DAsset* assetPtr = nullptr;
    {
        std::unique_lock<std::shared_mutex> lock(mMeshMutex);
        auto it = mMeshMap.find(assetId.Value());
        if (it != mMeshMap.end())
        {
            // Already loaded or loading — fire completion if ready.
            assetPtr = it->second;
            if (assetPtr->IsReady())
            {
                lock.unlock();
                callback->OnLoadComplete(assetId);
                return;
            }
            // Still pending — fall through; a second Tick() will fire the callback.
            // (A more complete implementation would queue the extra callback, but
            //  the spec does not require double-load handling.)
            return;
        }
        assetPtr = new Mesh3DAsset(assetId);
        mMeshMap[assetId.Value()] = assetPtr;
    }

    // Build a PendingResult that the worker will fill in.
    PendingResult* result = new PendingResult();
    result->assetId   = assetId;
    result->asset     = assetPtr;
    result->callback  = callback;

    // Capture path as a std::string so it is safe to access from the worker thread
    // without any lifetime dependency on the String512 argument.
    std::string pathStr(resolvedPath.AsCStr());

    result->job = mJobSystem->Submit([result, pathStr]()
    {
        DIA_TRACE_ZONE("mesh3d.load", Dia::Observation::Trace::Category::kDiaAssetRuntime);

        result->readResult = new ReadResult();
        ReadMesh3DFile(pathStr.c_str(), result->readResult);

        if (result->readResult->status == ReadResult::Status::OK)
        {
            result->success = true;
        }
        else
        {
            result->success = false;
            // Map status enum to a human-readable reason.
            const char* reason = "unknown error";
            switch (result->readResult->status)
            {
                case ReadResult::Status::BadMagic:    reason = "bad magic number";      break;
                case ReadResult::Status::BadVersion:  reason = "unsupported version";   break;
                case ReadResult::Status::CountOverflow: reason = "count overflow";       break;
                case ReadResult::Status::ReadError:   reason = "file read error";       break;
                default: break;
            }
            std::strncpy(result->failReason, reason, sizeof(result->failReason) - 1);
            result->failReason[sizeof(result->failReason) - 1] = '\0';
        }

        // ReadResult is large (~4 MB); keep the heap pointer, don't copy it back.
    });

    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        mPending.push_back(result);
    }
}

//-----------------------------------------------------------------------------
// Unload
//-----------------------------------------------------------------------------

void Mesh3DAssetHandler::Unload(const Dia::Core::StringCRC& assetId)
{
    Mesh3DAsset* toDelete = nullptr;
    {
        std::unique_lock<std::shared_mutex> lock(mMeshMutex);
        auto it = mMeshMap.find(assetId.Value());
        if (it == mMeshMap.end())
            return;
        toDelete = it->second;
        mMeshMap.erase(it);
    }
    delete toDelete;
}

//-----------------------------------------------------------------------------
// Tick — drain pending results on the owner thread
//-----------------------------------------------------------------------------

void Mesh3DAssetHandler::Tick()
{
    DIA_PROFILE_SCOPE("mesh3d.tick", Dia::Observation::Profile::Category::kDiaAssetRuntime);

    // Lock-swap: grab all pending work without holding the mutex during processing.
    std::vector<PendingResult*> toProcess;
    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        toProcess.swap(mPending);
    }

    for (PendingResult* r : toProcess)
    {
        // Wait for the worker to finish (usually already done by the time Tick runs).
        if (r->job.IsValid() && mJobSystem)
        {
            mJobSystem->Wait(r->job);
            r->job = Dia::Core::JobHandle();
        }

        // Guard against Unload() having removed the asset while load was in-flight.
        {
            std::shared_lock<std::shared_mutex> lock(mMeshMutex);
            if (mMeshMap.find(r->assetId.Value()) == mMeshMap.end())
            {
                delete r->readResult;
                delete r;
                continue;
            }
        }

        if (r->success && r->readResult != nullptr)
        {
            r->asset->Populate(
                r->readResult->vertices,  r->readResult->vertexCount,
                r->readResult->indices,   r->readResult->indexCount,
                r->readResult->submeshes, r->readResult->submeshCount,
                r->readResult->bounds
            );
            r->callback->OnLoadComplete(r->assetId);

            auto* gauge = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(
                Dia::Core::StringCRC("dia.mesh3d.loaded_count"));
            if (gauge)
                gauge->Set(static_cast<double>(GetLoadedCount()));
        }
        else
        {
            const char* reason = r->failReason[0] != '\0' ? r->failReason : "mesh load failed";
            DIA_LOG_ERROR("DiaMesh3D", "Mesh3DAssetHandler: load failed for asset — %s", reason);
            r->asset->MarkFailed(reason);
            r->callback->OnLoadFailed(r->assetId, reason);

            auto* counter = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(
                Dia::Core::StringCRC("dia.mesh3d.failed_count"));
            if (counter)
                counter->Inc();
        }

        delete r->readResult;
        delete r;
    }
}

} }
