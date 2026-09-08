#pragma once

namespace Dia
{
	namespace Editor
	{
		class IEditorPlugin;

		// Loads a .diaapp manifest and invokes a callback for each plugin entry found.
		// Pure utility — no DiaApplicationFlow dependency.
		// TODO: [Architecture] .diaapp serves dual purpose (application manifests and
		// editor plugin config). Whether editor plugins should use a separate format
		// (e.g., .diaeditor) is a future discussion.
		class EditorManifestLoader
		{
		public:
			struct PluginEntry
			{
				char typeId[128];
				char instanceId[128];
			};

			using PluginCallback = void(*)(const PluginEntry&, void* userData);

			// Returns false if the file could not be opened or parsed.
			static bool Load(const char* manifestPath, PluginCallback callback, void* userData);
		};
	}
}
