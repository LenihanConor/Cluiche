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
	}
}
