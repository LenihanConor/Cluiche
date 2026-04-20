#include "DiaEditor/Plugin/EditorPluginRegistry.h"

#include <DiaCore/Core/Assert.h>

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
		}

		IEditorPlugin* EditorPluginRegistry::CreatePlugin(const Dia::Core::StringCRC& typeId)
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].typeId == typeId)
				{
					return mEntries[i].factory->Create();
				}
			}
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
