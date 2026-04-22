#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaEditor/Plugin/IEditorPlugin.h"

namespace Dia
{
	namespace Editor
	{
		class EditorPluginRegistry
		{
		public:
			static EditorPluginRegistry& Instance();

			void RegisterPlugin(const Dia::Core::StringCRC& typeId, IEditorPluginFactory* factory);
			IEditorPlugin* CreatePlugin(const Dia::Core::StringCRC& typeId);
			bool IsPluginRegistered(const Dia::Core::StringCRC& typeId) const;

			unsigned int GetRegisteredCount() const;
			const Dia::Core::StringCRC& GetRegisteredTypeId(unsigned int index) const;
			IEditorPluginFactory* GetFactory(unsigned int index) const;

		private:
			EditorPluginRegistry() = default;
			~EditorPluginRegistry() = default;
			EditorPluginRegistry(const EditorPluginRegistry&) = delete;
			EditorPluginRegistry& operator=(const EditorPluginRegistry&) = delete;

			struct PluginEntry
			{
				Dia::Core::StringCRC typeId;
				IEditorPluginFactory* factory;
			};

			static const unsigned int kMaxPlugins = 32;
			Dia::Core::Containers::DynamicArrayC<PluginEntry, kMaxPlugins> mEntries;
		};
	}
}
