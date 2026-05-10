#include "DiaAssetRuntime/AssetRuntime.h"

#include "DiaCore/FilePath/PathStore.h"
#include "DiaCore/FilePath/Path.h"
#include "DiaCore/CRC/CRC.h"

#include <DiaLogger/DiaLog.h>

#include <math.h>
#include <string.h>
#include <cstdio>
#include <filesystem>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace
{
    unsigned int GetCurrentThreadIdValue()
    {
#if defined(_MSC_VER)
        return static_cast<unsigned int>(::GetCurrentThreadId());
#else
        return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(pthread_self()) & 0xFFFFFFFF);
#endif
    }

    const char* AssetStateToStr(Dia::AssetRuntime::AssetState s)
    {
        switch (s)
        {
            case Dia::AssetRuntime::AssetState::Null:     return "Null";
            case Dia::AssetRuntime::AssetState::Staged:   return "Staged";
            case Dia::AssetRuntime::AssetState::Loading:  return "Loading";
            case Dia::AssetRuntime::AssetState::Loaded:   return "Loaded";
            case Dia::AssetRuntime::AssetState::Failed:   return "Failed";
            case Dia::AssetRuntime::AssetState::Unloaded: return "Unloaded";
            default:                                       return "Unknown";
        }
    }
}

namespace Dia
{
    namespace AssetRuntime
    {
        //------------------------------------------------------------------------------------
        // StateHashFunctor
        //------------------------------------------------------------------------------------
        unsigned int AssetRuntime::StateHashFunctor::GetHashIndex(const Key& key, const TableData* tableData) const
        {
            static const unsigned int sTranslationToTableSpace =
                Dia::Core::CRC::MaxCRC() / AssetRuntime::kStateTableSize;
            DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");
            return static_cast<unsigned int>(floorf(static_cast<float>(key.Value()) /
                                                    static_cast<float>(sTranslationToTableSpace)));
        }

        //------------------------------------------------------------------------------------
        // AssetRuntime
        //------------------------------------------------------------------------------------
        AssetRuntime::AssetRuntime()
            : mOwnerThreadId(GetCurrentThreadIdValue())
        {}

        bool AssetRuntime::LoadManifest(const Dia::Core::FilePath& manifestPath)
        {
            AssertOwnerThread();
            RuntimeManifestLoader loader;
            if (!loader.Load(manifestPath, mAssetTable, mStageTable))
                return false;

            InitStateTable();
            InitRefCountTable();
            RegisterPathAliases();
            DIA_LOG_INFO("AssetRuntime", "LoadManifest: %u assets across %u stages registered",
                mAssetTable.Size(), mStageTable.Size());
            return true;
        }

        bool AssetRuntime::LoadManifest(const Dia::Core::FilePath::ResoledFilePath& resolvedManifestPath)
        {
            AssertOwnerThread();
            RuntimeManifestLoader loader;
            if (!loader.Load(resolvedManifestPath, mAssetTable, mStageTable))
                return false;

            InitStateTable();
            InitRefCountTable();
            RegisterPathAliases();
            DIA_LOG_INFO("AssetRuntime", "LoadManifest: %u assets across %u stages registered",
                mAssetTable.Size(), mStageTable.Size());
            return true;
        }

        const Dia::Core::Containers::String512* AssetRuntime::ResolveAssetPath(const Dia::Core::StringCRC& assetId) const
        {
            AssertOwnerThread();
            const RuntimeAssetEntry* entry = mAssetTable.TryGetItemConst(assetId);
            if (!entry)
            {
                DIA_LOG_WARNING("AssetRuntime", "ResolveAssetPath: unknown asset '%s'", assetId.AsChar());
                return nullptr;
            }
            return &entry->mDeployPath;
        }

        AssetState AssetRuntime::GetAssetState(const Dia::Core::StringCRC& assetId) const
        {
            AssertOwnerThread();
            const AssetState* state = mStateTable.TryGetItemConst(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "GetAssetState: unknown asset '%s'", assetId.AsChar());
                return AssetState::Null;
            }
            return *state;
        }

        bool AssetRuntime::IsAssetReady(const Dia::Core::StringCRC& assetId) const
        {
            AssertOwnerThread();
            const AssetState* state = mStateTable.TryGetItemConst(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "IsAssetReady: unknown asset '%s'", assetId.AsChar());
                return false;
            }
            return *state == AssetState::Loaded;
        }

        bool AssetRuntime::IsLoadComplete(const Dia::Core::StringCRC& stageId) const
        {
            AssertOwnerThread();
            const RuntimeStageEntry* stage = mStageTable.TryGetItemConst(stageId);
            if (!stage)
            {
                DIA_LOG_WARNING("AssetRuntime", "IsLoadComplete: unknown stage '%s'", stageId.AsChar());
                return false;
            }

            unsigned int assetCount = stage->mAssetIds.Size();
            for (unsigned int i = 0; i < assetCount; ++i)
            {
                const AssetState* state = mStateTable.TryGetItemConst(stage->mAssetIds[i]);
                if (!state || *state != AssetState::Loaded)
                    return false;
            }
            return true;
        }

        void AssetRuntime::RequestStageLoad(const Dia::Core::StringCRC& stageId)
        {
            AssertOwnerThread();
            const RuntimeStageEntry* stage = mStageTable.TryGetItemConst(stageId);
            if (!stage)
            {
                DIA_LOG_WARNING("AssetRuntime", "RequestStageLoad: unknown stage '%s'", stageId.AsChar());
                return;
            }

            DIA_LOG_INFO("AssetRuntime", "RequestStageLoad: stage '%s' (%u assets)",
                stageId.AsChar(), stage->mAssetIds.Size());

            unsigned int assetCount = stage->mAssetIds.Size();
            for (unsigned int i = 0; i < assetCount; ++i)
            {
                const Dia::Core::StringCRC& assetId = stage->mAssetIds[i];

                unsigned int* refCount = mRefCountTable.TryGetItem(assetId);
                if (!refCount)
                {
                    DIA_LOG_WARNING("AssetRuntime", "RequestStageLoad: asset '%s' in stage '%s' not found in ref table",
                        assetId.AsChar(), stageId.AsChar());
                    continue;
                }

                unsigned int prev = *refCount;
                (*refCount)++;

                if (prev == 0)
                {
                    if (TryTransition(assetId, AssetState::Staged))
                    {
                        DispatchLoad(assetId);
                    }
                }
            }
        }

        void AssetRuntime::RequestStageUnload(const Dia::Core::StringCRC& stageId)
        {
            AssertOwnerThread();
            const RuntimeStageEntry* stage = mStageTable.TryGetItemConst(stageId);
            if (!stage)
            {
                DIA_LOG_WARNING("AssetRuntime", "RequestStageUnload: unknown stage '%s'", stageId.AsChar());
                return;
            }

            unsigned int assetCount = stage->mAssetIds.Size();
            for (unsigned int i = 0; i < assetCount; ++i)
            {
                const Dia::Core::StringCRC& assetId = stage->mAssetIds[i];

                const AssetState* state = mStateTable.TryGetItemConst(assetId);
                if (state && *state == AssetState::Loading)
                {
                    DIA_ASSERT(false, "RequestStageUnload: asset '%s' is in Loading state — cannot unload while loading (programming error in game flow)", assetId.AsChar());
                    continue;
                }

                unsigned int* refCount = mRefCountTable.TryGetItem(assetId);
                if (!refCount)
                {
                    DIA_LOG_WARNING("AssetRuntime", "RequestStageUnload: asset '%s' in stage '%s' not found in ref table",
                        assetId.AsChar(), stageId.AsChar());
                    continue;
                }

                if (*refCount == 0)
                {
                    DIA_LOG_WARNING("AssetRuntime",
                        "RequestStageUnload: asset '%s' ref count already 0 (double unload for stage '%s')",
                        assetId.AsChar(), stageId.AsChar());
                    continue;
                }

                (*refCount)--;

                if (*refCount == 0)
                {
                    DispatchUnload(assetId);
                    TryTransition(assetId, AssetState::Unloaded);
                }
            }
        }

        void AssetRuntime::RegisterTypeHandler(const char* typePrefix, IAssetTypeHandler* handler)
        {
            AssertOwnerThread();
            if (!typePrefix || !handler)
            {
                DIA_LOG_WARNING("AssetRuntime", "RegisterTypeHandler: null argument");
                return;
            }

            Dia::Core::StringCRC prefixCRC(typePrefix);

            for (unsigned int i = 0; i < mHandlers.Size(); ++i)
            {
                if (mHandlers[i].mTypePrefixCRC == prefixCRC)
                {
                    DIA_LOG_WARNING("AssetRuntime", "RegisterTypeHandler: handler already registered for '%s'", typePrefix);
                    return;
                }
            }

            if (mHandlers.Size() == kMaxHandlers)
            {
                DIA_LOG_WARNING("AssetRuntime", "RegisterTypeHandler: handler list full (capacity %u)", kMaxHandlers);
                return;
            }

            HandlerEntry entry;
            entry.mTypePrefixCRC = prefixCRC;
            entry.mTypePrefix = Dia::Core::Containers::String32(typePrefix);
            entry.mHandler = handler;
            mHandlers.Add(entry);
        }

        void AssetRuntime::UnregisterTypeHandler(const char* typePrefix)
        {
            AssertOwnerThread();
            if (!typePrefix)
                return;

            Dia::Core::StringCRC prefixCRC(typePrefix);
            for (unsigned int i = 0; i < mHandlers.Size(); ++i)
            {
                if (mHandlers[i].mTypePrefixCRC == prefixCRC)
                {
                    mHandlers.RemoveAt(i);
                    return;
                }
            }

            DIA_LOG_WARNING("AssetRuntime", "UnregisterTypeHandler: no handler registered for '%s'", typePrefix);
        }

        void AssetRuntime::RetryAssetLoad(const Dia::Core::StringCRC& assetId)
        {
            AssertOwnerThread();
            const AssetState* state = mStateTable.TryGetItemConst(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "RetryAssetLoad: unknown asset '%s'", assetId.AsChar());
                return;
            }

            if (*state != AssetState::Failed)
            {
                DIA_LOG_WARNING("AssetRuntime", "RetryAssetLoad: asset '%s' is not in Failed state", assetId.AsChar());
                return;
            }

            if (TryTransition(assetId, AssetState::Loading))
            {
                DispatchLoad(assetId);
            }
        }

        AssetRuntime::LoadProgress AssetRuntime::GetLoadProgress(const Dia::Core::StringCRC& stageId) const
        {
            AssertOwnerThread();
            LoadProgress progress = { 0, 0, 0 };

            const RuntimeStageEntry* stage = mStageTable.TryGetItemConst(stageId);
            if (!stage)
            {
                DIA_LOG_WARNING("AssetRuntime", "GetLoadProgress: unknown stage '%s'", stageId.AsChar());
                return progress;
            }

            progress.total = stage->mAssetIds.Size();
            for (unsigned int i = 0; i < progress.total; ++i)
            {
                const AssetState* state = mStateTable.TryGetItemConst(stage->mAssetIds[i]);
                if (!state)
                    continue;

                if (*state == AssetState::Loaded)
                    progress.loaded++;
                else if (*state == AssetState::Failed)
                    progress.failed++;
            }

            return progress;
        }

        unsigned int AssetRuntime::GetAllAssets(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const
        {
            AssertOwnerThread();
            unsigned int total = mAssetTable.Size();
            for (unsigned int i = 0; i < total; ++i)
            {
                if (!results.IsFull())
                    results.Add(mAssetTable.GetItemByIndexConst(i).mId);
            }
            return total;
        }

        unsigned int AssetRuntime::GetLoadedAssets(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const
        {
            AssertOwnerThread();
            unsigned int total = 0;
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                const AssetState* state = mStateTable.TryGetItemConst(entry.mId);
                if (state && *state == AssetState::Loaded)
                {
                    total++;
                    if (!results.IsFull())
                        results.Add(entry.mId);
                }
            }
            return total;
        }

        unsigned int AssetRuntime::GetStagedAssets(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const
        {
            AssertOwnerThread();
            unsigned int total = 0;
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                const AssetState* state = mStateTable.TryGetItemConst(entry.mId);
                if (state && *state == AssetState::Staged)
                {
                    total++;
                    if (!results.IsFull())
                        results.Add(entry.mId);
                }
            }
            return total;
        }

        unsigned int AssetRuntime::GetStageDependencies(
            const Dia::Core::StringCRC& stageId,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const
        {
            AssertOwnerThread();
            const RuntimeStageEntry* stage = mStageTable.TryGetItemConst(stageId);
            if (!stage)
            {
                DIA_LOG_WARNING("AssetRuntime", "GetStageDependencies: unknown stage '%s'", stageId.AsChar());
                return 0;
            }

            unsigned int total = stage->mAssetIds.Size();
            for (unsigned int i = 0; i < total; ++i)
            {
                if (!results.IsFull())
                    results.Add(stage->mAssetIds[i]);
            }
            return total;
        }

        void AssetRuntime::Reset()
        {
            AssertOwnerThread();

            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                AssetState* state = mStateTable.TryGetItem(entry.mId);
                if (!state) continue;

                if (*state == AssetState::Loaded || *state == AssetState::Loading)
                    DispatchUnload(entry.mId);
            }

            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                AssetState* state = mStateTable.TryGetItem(entry.mId);
                if (state) *state = AssetState::Null;

                unsigned int* refCount = mRefCountTable.TryGetItem(entry.mId);
                if (refCount) *refCount = 0u;
            }
        }

        unsigned int AssetRuntime::GetAssetRefCount(const Dia::Core::StringCRC& assetId) const
        {
            AssertOwnerThread();
            const unsigned int* refCount = mRefCountTable.TryGetItemConst(assetId);
            if (!refCount)
            {
                DIA_LOG_WARNING("AssetRuntime", "GetAssetRefCount: unknown asset '%s'", assetId.AsChar());
                return 0;
            }
            return *refCount;
        }

        AssetScope AssetRuntime::GetAssetScope(const Dia::Core::StringCRC& assetId) const
        {
            AssertOwnerThread();
            const RuntimeAssetEntry* entry = mAssetTable.TryGetItemConst(assetId);
            if (!entry)
            {
                DIA_LOG_WARNING("AssetRuntime", "GetAssetScope: asset '%s' not found, defaulting to kGlobal", assetId.AsChar());
                return AssetScope::kGlobal;
            }
            return entry->mScope;
        }

        Dia::Core::StringCRC AssetRuntime::GetAssetStageId(const Dia::Core::StringCRC& assetId) const
        {
            AssertOwnerThread();
            unsigned int stageCount = mStageTable.Size();
            for (unsigned int i = 0; i < stageCount; ++i)
            {
                const RuntimeStageEntry& stage = mStageTable.GetItemByIndexConst(i);
                unsigned int assetCount = stage.mAssetIds.Size();
                for (unsigned int j = 0; j < assetCount; ++j)
                {
                    if (stage.mAssetIds[j] == assetId)
                        return stage.mId;
                }
            }
            return Dia::Core::StringCRC();
        }

        //------------------------------------------------------------------------------------
        // IAssetLoadCallback
        //------------------------------------------------------------------------------------
        void AssetRuntime::OnLoadComplete(const Dia::Core::StringCRC& assetId)
        {
            DIA_LOG_INFO("AssetRuntime", "Asset '%s' loaded", assetId.AsChar());
            TryTransition(assetId, AssetState::Loaded);
        }

        void AssetRuntime::OnLoadFailed(const Dia::Core::StringCRC& assetId, const char* reason)
        {
            DIA_LOG_ERROR("AssetRuntime", "Asset '%s' load failed: %s", assetId.AsChar(), reason ? reason : "unknown");
            TryTransition(assetId, AssetState::Failed);
        }

        //------------------------------------------------------------------------------------
        // Private
        //------------------------------------------------------------------------------------
        void AssetRuntime::InitStateTable()
        {
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                mStateTable.Add(entry.mId, AssetState::Null);
            }
        }

        void AssetRuntime::InitRefCountTable()
        {
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                mRefCountTable.Add(entry.mId, 0u);
            }
        }

        void AssetRuntime::RegisterPathAliases()
        {
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                const Dia::Core::StringCRC& id = entry.mId;
                const char* pathStr = entry.mDeployPath.AsCStr();

                if (Dia::Core::PathStore::IsPathAliasRegistered(id))
                {
                    DIA_LOG_WARNING("AssetRuntime", "RegisterPathAliases: alias '%s' already registered — skipping", id.AsChar());
                    continue;
                }

                Dia::Core::Path::String pathValue(pathStr);
                Dia::Core::PathStore::RegisterToStore(id, pathValue);
            }
        }

        void AssetRuntime::DispatchLoad(const Dia::Core::StringCRC& assetId)
        {
            const AssetState* currentState = mStateTable.TryGetItemConst(assetId);
            if (!currentState || *currentState != AssetState::Loading)
                TryTransition(assetId, AssetState::Loading);

            const RuntimeAssetEntry* entry = mAssetTable.TryGetItemConst(assetId);
            if (!entry)
            {
                DIA_LOG_ERROR("AssetRuntime", "DispatchLoad: asset '%s' not in table", assetId.AsChar());
                TryTransition(assetId, AssetState::Failed);
                return;
            }

            Dia::Core::StringCRC typePrefixCRC = ExtractTypePrefix(assetId);
            IAssetTypeHandler* handler = FindHandler(typePrefixCRC);

            if (handler)
            {
                handler->Load(assetId, entry->mDeployPath, this);
            }
            else
            {
                AutoValidate(assetId, entry->mDeployPath);
            }
        }

        void AssetRuntime::AutoValidate(const Dia::Core::StringCRC& assetId,
                                         const Dia::Core::Containers::String512& resolvedPath)
        {
            const char* path = resolvedPath.AsCStr();
            bool exists = false;

            // Folder assets have trailing '/' in their deploy path
            unsigned int len = resolvedPath.Length();
            if (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\'))
            {
                exists = std::filesystem::is_directory(path);
            }
            else
            {
                FILE* f = nullptr;
                fopen_s(&f, path, "rb");
                if (f)
                {
                    fclose(f);
                    exists = true;
                }
            }

            if (exists)
            {
                DIA_LOG_INFO("AssetRuntime", "Asset '%s' auto-validated at '%s'", assetId.AsChar(), path);
                TryTransition(assetId, AssetState::Loaded);
            }
            else
            {
                DIA_LOG_ERROR("AssetRuntime", "AutoValidate: resource not found at '%s' for asset '%s'", path, assetId.AsChar());
                TryTransition(assetId, AssetState::Failed);
            }
        }

        void AssetRuntime::DispatchUnload(const Dia::Core::StringCRC& assetId)
        {
            Dia::Core::StringCRC typePrefixCRC = ExtractTypePrefix(assetId);
            IAssetTypeHandler* handler = FindHandler(typePrefixCRC);

            if (handler)
            {
                handler->Unload(assetId);
            }
        }

        Dia::Core::StringCRC AssetRuntime::ExtractTypePrefix(const Dia::Core::StringCRC& assetId) const
        {
            const char* idStr = assetId.AsChar();
            if (!idStr)
                return Dia::Core::StringCRC();

            char prefix[32];
            unsigned int i = 0;
            while (idStr[i] != '\0' && idStr[i] != '.' && i < 31)
            {
                prefix[i] = idStr[i];
                i++;
            }
            prefix[i] = '\0';

            return Dia::Core::StringCRC(prefix);
        }

        IAssetTypeHandler* AssetRuntime::FindHandler(const Dia::Core::StringCRC& typePrefixCRC) const
        {
            for (unsigned int i = 0; i < mHandlers.Size(); ++i)
            {
                if (mHandlers[i].mTypePrefixCRC == typePrefixCRC)
                    return mHandlers[i].mHandler;
            }
            return nullptr;
        }

        bool AssetRuntime::TryTransition(const Dia::Core::StringCRC& assetId, AssetState target)
        {
            AssetState* state = mStateTable.TryGetItem(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "TryTransition: unknown asset '%s'", assetId.AsChar());
                return false;
            }

            AssetState current = *state;

            bool valid = false;
            switch (current)
            {
                case AssetState::Null:     valid = (target == AssetState::Staged);                break;
                case AssetState::Staged:   valid = (target == AssetState::Loading);               break;
                case AssetState::Loading:  valid = (target == AssetState::Loaded)
                                                || (target == AssetState::Failed);                break;
                case AssetState::Loaded:   valid = (target == AssetState::Unloaded);              break;
                case AssetState::Failed:   valid = (target == AssetState::Loading)
                                                || (target == AssetState::Unloaded);              break;
                case AssetState::Unloaded: valid = (target == AssetState::Staged);                break;
            }

            if (!valid)
            {
                DIA_LOG_WARNING("AssetRuntime",
                    "TryTransition: invalid transition for '%s': %d -> %d",
                    assetId.AsChar(),
                    static_cast<int>(current),
                    static_cast<int>(target));
                return false;
            }

            DIA_LOG_INFO("AssetRuntime",
                "State change: '%s' %s -> %s",
                assetId.AsChar(),
                AssetStateToStr(current),
                AssetStateToStr(target));

            *state = target;
            return true;
        }

        void AssetRuntime::AssertOwnerThread() const
        {
            DIA_ASSERT(GetCurrentThreadIdValue() == mOwnerThreadId,
                "AssetRuntime accessed from wrong thread — single-threaded use only");
        }

    } // namespace AssetRuntime
} // namespace Dia
