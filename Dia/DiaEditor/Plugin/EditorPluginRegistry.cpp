#include "DiaEditor/Plugin/EditorPluginRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

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

			DIA_LOG_INFO("Editor", "EditorPluginRegistry: Registered plugin (count=%u)", mEntries.Size());
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
	}
}
