#pragma once

#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Dia { namespace Mesh3D {

// Async .mesh3d asset loader. Worker thread reads binary data from disk;
// Tick() (owner thread) calls Populate/MarkFailed and fires callbacks.
//
// Thread contract:
//   Load()          — any thread
//   Unload()        — any thread
//   Tick()          — owner thread only
//   LookupMesh()    — any thread (shared lock)
//   GetLoadedCount() — any thread (shared lock)
//
// Callbacks fire on the owner thread during Tick().
class Mesh3DAssetHandler : public Dia::AssetRuntime::IAssetTypeHandler
{
public:
    Mesh3DAssetHandler();
    ~Mesh3DAssetHandler();

    void SetJobSystem(Dia::Core::JobSystem* jobSystem);

    // Optional callback fired on Unload() so GPU-side caches can evict in sync.
    // Called from whatever thread Unload() is called on — caller is responsible
    // for thread safety on the GPU cache side.
    using EvictCallback = std::function<void(uint32_t assetId)>;
    void SetEvictCallback(EvictCallback callback);

    // Thread-safe lookup. Returns nullptr if not loaded.
    Mesh3DAsset* LookupMesh(Dia::Core::StringCRC assetId) const;
    unsigned int GetLoadedCount() const;

    // Register a pre-built asset (ownership transferred to handler).
    // Replaces any existing asset with the same ID. Thread-safe.
    void RegisterMesh(Mesh3DAsset* asset);

    // IAssetTypeHandler
    void Load(const Dia::Core::StringCRC& assetId,
              const Dia::Core::Containers::String512& resolvedPath,
              Dia::AssetRuntime::IAssetLoadCallback* callback) override;

    void Unload(const Dia::Core::StringCRC& assetId) override;

    // Drain worker results on owner thread; fires OnLoadComplete/OnLoadFailed.
    void Tick();

private:
    // PendingResult is fully defined in the .cpp (includes ReadResult from Mesh3DBinaryReader.h).
    struct PendingResult;

    mutable std::shared_mutex                          mMeshMutex;
    std::unordered_map<unsigned int, Mesh3DAsset*>     mMeshMap;   // owned raw ptrs

    std::mutex                                         mPendingMutex;
    std::vector<PendingResult*>                        mPending;   // heap-allocated

    Dia::Core::JobSystem* mJobSystem = nullptr;
    EvictCallback         mEvictCallback;
};

} }
