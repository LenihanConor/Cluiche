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
			static const unsigned int kMaxPlugins = 32;
			static const unsigned int kMaxManifests = 16;

			static EditorPluginRegistry& Instance();

			void RegisterPlugin(const Dia::Core::StringCRC& typeId, IEditorPluginFactory* factory);
			IEditorPlugin* CreatePlugin(const Dia::Core::StringCRC& typeId);
			bool IsPluginRegistered(const Dia::Core::StringCRC& typeId) const;

			unsigned int GetRegisteredCount() const;
			const Dia::Core::StringCRC& GetRegisteredTypeId(unsigned int index) const;
			IEditorPluginFactory* GetFactory(unsigned int index) const;

			void TagPluginManifest(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& manifestId);
			void SetActiveManifests(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxManifests>& manifests);
			void ClearActiveManifests();
			bool IsInScopeFilter(const Dia::Core::StringCRC& typeId) const;

		private:
			EditorPluginRegistry() = default;
			~EditorPluginRegistry() = default;
			EditorPluginRegistry(const EditorPluginRegistry&) = delete;
			EditorPluginRegistry& operator=(const EditorPluginRegistry&) = delete;

			struct PluginEntry
			{
				Dia::Core::StringCRC typeId;
				IEditorPluginFactory* factory;
				Dia::Core::StringCRC manifestId;  // empty = built-in
			};

			Dia::Core::Containers::DynamicArrayC<PluginEntry, kMaxPlugins> mEntries;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxManifests> mActiveManifests;
		};
	}
}
