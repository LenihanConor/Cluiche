#include "DiaAssetRuntime/AssetRuntime.h"

#include "DiaCore/FilePath/PathStore.h"
#include "DiaCore/FilePath/Path.h"
#include "DiaCore/CRC/CRC.h"

#include <DiaLogger/DiaLog.h>

#include <math.h>

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
            : mIsDispatching(false)
            , mOwnerThreadId(GetCurrentThreadIdValue())
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
                return AssetState::Registered;
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

        void AssetRuntime::AcknowledgeAssetLoaded(const Dia::Core::StringCRC& assetId)
        {
            AssertOwnerThread();
            TryTransition(assetId, AssetState::Loaded);
        }

        void AssetRuntime::AcknowledgeAssetUnloaded(const Dia::Core::StringCRC& assetId)
        {
            AssertOwnerThread();
            TryTransition(assetId, AssetState::Registered);
        }

        void AssetRuntime::AcknowledgeAssetLoadFailed(const Dia::Core::StringCRC& assetId)
        {
            AssertOwnerThread();
            TryTransition(assetId, AssetState::Registered);
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

                if (*state != AssetState::Registered)
                    DispatchAssetUnloading(entry.mId);
            }

            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                AssetState* state = mStateTable.TryGetItem(entry.mId);
                if (state) *state = AssetState::Registered;

                unsigned int* refCount = mRefCountTable.TryGetItem(entry.mId);
                if (refCount) *refCount = 0u;
            }
        }

        void AssetRuntime::RegisterListener(IAssetStateListener* listener)
        {
            AssertOwnerThread();
            if (!listener)
                return;

            unsigned int count = mListeners.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                if (mListeners[i] == listener)
                {
                    DIA_LOG_WARNING("AssetRuntime", "RegisterListener: listener already registered");
                    return;
                }
            }

            if (mListeners.Size() == kMaxListeners)
            {
                DIA_LOG_WARNING("AssetRuntime", "RegisterListener: listener list full (capacity %u)", kMaxListeners);
                return;
            }

            mListeners.Add(listener);
        }

        void AssetRuntime::UnregisterListener(IAssetStateListener* listener)
        {
            AssertOwnerThread();
            if (!listener)
                return;

            if (mIsDispatching)
            {
                if (mDeferredRemovals.Size() < kMaxListeners)
                    mDeferredRemovals.Add(listener);
                return;
            }

            unsigned int count = mListeners.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                if (mListeners[i] == listener)
                {
                    mListeners.RemoveAt(i);
                    return;
                }
            }

            DIA_LOG_WARNING("AssetRuntime", "UnregisterListener: listener not found");
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
                    TryTransition(assetId, AssetState::Staged);
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
                    TryTransition(assetId, AssetState::Unloading);
                }
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
                return AssetScope::kGlobal;
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
        // Private
        //------------------------------------------------------------------------------------
        void AssetRuntime::InitStateTable()
        {
            // Initialize all loaded assets to Registered state.
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                mStateTable.Add(entry.mId, AssetState::Registered);
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

        void AssetRuntime::DispatchAssetReady(const Dia::Core::StringCRC& assetId)
        {
            const RuntimeAssetEntry* entry = mAssetTable.TryGetItemConst(assetId);
            if (!entry)
                return;

            mIsDispatching = true;
            unsigned int count = mListeners.Size();
            for (unsigned int i = 0; i < count; ++i)
                mListeners[i]->OnAssetReady(assetId, entry->mDeployPath);
            mIsDispatching = false;

            FlushDeferredRemovals();
        }

        void AssetRuntime::DispatchAssetUnloading(const Dia::Core::StringCRC& assetId)
        {
            mIsDispatching = true;
            unsigned int count = mListeners.Size();
            for (unsigned int i = 0; i < count; ++i)
                mListeners[i]->OnAssetUnloading(assetId);
            mIsDispatching = false;

            FlushDeferredRemovals();
        }

        void AssetRuntime::DispatchAssetLoadFailed(const Dia::Core::StringCRC& assetId)
        {
            mIsDispatching = true;
            unsigned int count = mListeners.Size();
            for (unsigned int i = 0; i < count; ++i)
                mListeners[i]->OnAssetLoadFailed(assetId);
            mIsDispatching = false;

            FlushDeferredRemovals();
        }

        void AssetRuntime::FlushDeferredRemovals()
        {
            unsigned int removeCount = mDeferredRemovals.Size();
            for (unsigned int i = 0; i < removeCount; ++i)
            {
                IAssetStateListener* listener = mDeferredRemovals[i];
                unsigned int count = mListeners.Size();
                for (unsigned int j = 0; j < count; ++j)
                {
                    if (mListeners[j] == listener)
                    {
                        mListeners.RemoveAt(j);
                        break;
                    }
                }
            }
            mDeferredRemovals.RemoveAll();
        }

        void AssetRuntime::AssertOwnerThread() const
        {
            DIA_ASSERT(GetCurrentThreadIdValue() == mOwnerThreadId,
                "AssetRuntime accessed from wrong thread — single-threaded use only");
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

            // Validate allowed transitions
            bool valid = false;
            switch (current)
            {
                case AssetState::Registered: valid = (target == AssetState::Staged);       break;
                case AssetState::Staged:     valid = (target == AssetState::Loaded)
                                                  || (target == AssetState::Unloading)
                                                  || (target == AssetState::Registered); break;
                case AssetState::Loaded:     valid = (target == AssetState::Unloading);   break;
                case AssetState::Unloading:  valid = (target == AssetState::Registered);  break;
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

            DIA_LOG_DEBUG("AssetRuntime",
                "TryTransition: '%s' %d -> %d",
                assetId.AsChar(),
                static_cast<int>(current),
                static_cast<int>(target));

            *state = target;

            if (target == AssetState::Staged)
                DispatchAssetReady(assetId);
            else if (target == AssetState::Unloading)
                DispatchAssetUnloading(assetId);
            else if (target == AssetState::Registered && current == AssetState::Staged)
                DispatchAssetLoadFailed(assetId);

            return true;
        }

    } // namespace AssetRuntime
} // namespace Dia
