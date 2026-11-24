#include "DiaEditor/Plugin/EditorPluginRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
	namespace Editor
	{
		EditorPluginRegistry& EditorPluginRegistry::Instance()
		{
			static EditorPluginRegistry instance;
			return instance;
		}

		void EditorPluginRegistry::RegisterPlugin(const Dia::Core::StringCRC& typeId, IEditorPluginFactory* factory)
		{
			DIA_ASSERT(factory != nullptr, "EditorPluginRegistry: factory must not be null");
			DIA_ASSERT(!IsPluginRegistered(typeId), "EditorPluginRegistry: plugin type already registered");
			DIA_ASSERT(!mEntries.IsFull(), "EditorPluginRegistry: max plugin capacity reached");

			PluginEntry entry;
			entry.typeId = typeId;
			entry.factory = factory;
			mEntries.Add(entry);

			// Note: typeId is logged here, not the display name, because this runs at static-init
			// time before the log/observation subsystems are guaranteed up — and GetPluginInfo()
			// would construct a temp plugin instance early. Display names are logged at PluginLoaderModule
			// startup (RestoreLayoutPlugins) when the registry is enumerated against the saved layout.
			DIA_LOG_INFO("Editor", "EditorPluginRegistry: Registered typeId='%s' (count=%u)", typeId.AsChar(), mEntries.Size());
		}

		IEditorPlugin* EditorPluginRegistry::CreatePlugin(const Dia::Core::StringCRC& typeId)
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					DIA_LOG_INFO("Editor", "EditorPluginRegistry: Created plugin (entry %u of %u)", i, mEntries.Size());
					return mEntries[i].factory->Create();
				}
			}
			DIA_LOG_WARNING("Editor", "EditorPluginRegistry: type not found (checked %u entries)", mEntries.Size());
			return nullptr;
		}

		bool EditorPluginRegistry::IsPluginRegistered(const Dia::Core::StringCRC& typeId) const
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					return true;
				}
			}
			return false;
		}

		unsigned int EditorPluginRegistry::GetRegisteredCount() const
		{
			return mEntries.Size();
		}

		const Dia::Core::StringCRC& EditorPluginRegistry::GetRegisteredTypeId(unsigned int index) const
		{
			DIA_ASSERT(index < mEntries.Size(), "EditorPluginRegistry: index out of range");
			return mEntries[index].typeId;
		}

		IEditorPluginFactory* EditorPluginRegistry::GetFactory(unsigned int index) const
		{
			DIA_ASSERT(index < mEntries.Size(), "EditorPluginRegistry: index out of range");
			return mEntries[index].factory;
		}

		void EditorPluginRegistry::TagPluginManifest(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& manifestId)
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					mEntries[i].manifestId = manifestId;
					return;
				}
			}
			DIA_ASSERT(false, "EditorPluginRegistry::TagPluginManifest: typeId not found");
		}

		void EditorPluginRegistry::SetActiveManifests(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxManifests>& manifests)
		{
			mActiveManifests = manifests;
			DIA_LOG_INFO("Editor", "EditorPluginRegistry: SetActiveManifests count=%u", mActiveManifests.Size());
		}

		void EditorPluginRegistry::ClearActiveManifests()
		{
			mActiveManifests.RemoveAll();
			DIA_LOG_INFO("Editor", "EditorPluginRegistry: ClearActiveManifests");
		}

		bool EditorPluginRegistry::IsInScopeFilter(const Dia::Core::StringCRC& typeId) const
		{
			// No filter active — all plugins pass (cold-start)
			if (mActiveManifests.IsEmpty())
			{
				return true;
			}

			// Find the entry to check its manifestId
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					// Built-in plugins (empty manifestId) always pass
					if (mEntries[i].manifestId == Dia::Core::StringCRC())
					{
						return true;
					}

					// Check if plugin's manifest is in the active set
					for (unsigned int j = 0; j < mActiveManifests.Size(); ++j)
					{
						if (mActiveManifests[j] == mEntries[i].manifestId)
						{
							return true;
						}
					}
					return false;
				}
			}

			// typeId not registered — does not pass
			return false;
		}

		void EditorPluginRegistry::TagGameScoped(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& gameId)
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					mEntries[i].gameId = gameId;
					return;
				}
			}
			DIA_ASSERT(false, "EditorPluginRegistry::TagGameScoped: typeId not found");
		}

		void EditorPluginRegistry::SetGameScope(const Dia::Core::StringCRC& gameId, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxPlugins>& scopedTypeIds)
		{
			DIA_LOG_INFO("Editor", "EditorPluginRegistry: SetGameScope game=%s types=%u", gameId.AsChar(), scopedTypeIds.Size());
			for (unsigned int i = 0; i < scopedTypeIds.Size(); ++i)
			{
				TagGameScoped(scopedTypeIds[i], gameId);
			}
			ChangeActiveGame(gameId);
		}

		void EditorPluginRegistry::ClearGameScope()
		{
			if (mActiveGameId != Dia::Core::StringCRC())
			{
				DIA_LOG_INFO("Editor", "EditorPluginRegistry: ClearGameScope");
			}
			ChangeActiveGame(Dia::Core::StringCRC());
		}

		void EditorPluginRegistry::ChangeActiveGame(const Dia::Core::StringCRC& newGame)
		{
			if (mActiveGameId == newGame)
			{
				return;
			}

			const Dia::Core::StringCRC oldGame = mActiveGameId;
			mActiveGameId = newGame;
			for (unsigned int i = 0; i < mListenerCount; ++i)
			{
				mListeners[i].callback(mListeners[i].userData, oldGame, newGame);
			}
		}

		const Dia::Core::StringCRC& EditorPluginRegistry::GetActiveGameId() const
		{
			return mActiveGameId;
		}

		bool EditorPluginRegistry::IsGameScoped(const Dia::Core::StringCRC& typeId) const
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					return mEntries[i].gameId != Dia::Core::StringCRC();
				}
			}
			return false;
		}

		bool EditorPluginRegistry::IsAvailable(const Dia::Core::StringCRC& typeId) const
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					if (mEntries[i].gameId != Dia::Core::StringCRC())
					{
						return mEntries[i].gameId == mActiveGameId;
					}
					return IsInScopeFilter(typeId);
				}
			}
			return false;
		}

		bool EditorPluginRegistry::IsOutOfGameScope(const Dia::Core::StringCRC& typeId) const
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
					return mEntries[i].gameId != Dia::Core::StringCRC() && mEntries[i].gameId != mActiveGameId;
			}
			return false;
		}

		bool EditorPluginRegistry::AddAssetRoute(const Dia::Core::StringCRC& assetType, const Dia::Core::StringCRC& editorPluginType)
			{
				for (unsigned int i = 0; i < mAssetRoutes.Size(); ++i)
				{
					if (mAssetRoutes[i].assetType == assetType && mAssetRoutes[i].editorPluginType == editorPluginType)
						return false;
				}
				if (mAssetRoutes.IsFull())
				{
					DIA_LOG_WARNING("Editor", "EditorPluginRegistry: asset route capacity (%u) reached", kMaxAssetRoutes);
					return false;
				}
				AssetRoute route;
				route.assetType = assetType;
				route.editorPluginType = editorPluginType;
				mAssetRoutes.Add(route);
				return true;
			}

			bool EditorPluginRegistry::RemoveAssetRoute(const Dia::Core::StringCRC& assetType, const Dia::Core::StringCRC& editorPluginType)
			{
				for (unsigned int i = 0; i < mAssetRoutes.Size(); ++i)
				{
					if (mAssetRoutes[i].assetType == assetType && mAssetRoutes[i].editorPluginType == editorPluginType)
					{
						mAssetRoutes.RemoveAt(i);
						return true;
					}
				}
				return false;
			}

			void EditorPluginRegistry::ClearAssetRoutes()
			{
				mAssetRoutes.RemoveAll();
			}

			unsigned int EditorPluginRegistry::GetAssetRouteCount() const
			{
				return mAssetRoutes.Size();
			}

			const Dia::Core::StringCRC& EditorPluginRegistry::GetAssetRouteAssetType(unsigned int index) const
			{
				DIA_ASSERT(index < mAssetRoutes.Size(), "EditorPluginRegistry: asset route index out of range");
				return mAssetRoutes[index].assetType;
			}

			const Dia::Core::StringCRC& EditorPluginRegistry::GetAssetRouteEditor(unsigned int index) const
			{
				DIA_ASSERT(index < mAssetRoutes.Size(), "EditorPluginRegistry: asset route index out of range");
				return mAssetRoutes[index].editorPluginType;
			}

			void EditorPluginRegistry::AddGameScopeListener(GameScopeChangedCallback callback, void* userData)
		{
			if (callback == nullptr)
				return;
			DIA_ASSERT(mListenerCount < kMaxGameScopeListeners, "EditorPluginRegistry: max game scope listeners reached");
			if (mListenerCount >= kMaxGameScopeListeners)
				return;
			mListeners[mListenerCount].callback = callback;
			mListeners[mListenerCount].userData = userData;
			++mListenerCount;
		}

		void EditorPluginRegistry::RemoveGameScopeListener(GameScopeChangedCallback callback, void* userData)
		{
			for (unsigned int i = 0; i < mListenerCount; ++i)
			{
				if (mListeners[i].callback == callback && mListeners[i].userData == userData)
				{
					mListeners[i] = mListeners[mListenerCount - 1];
					--mListenerCount;
					return;
				}
			}
		}
	}
}
