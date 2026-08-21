#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
	}

	namespace SchemaBrowser
	{
		// Read-only, offline schema browser for *.diagamemessages declaration files.
		//
		// Discovery + JSON parsing happens in C++; ALL semantic interpretation
		// (graph building, duplicate detection, orphan analysis) lives in the TS
		// UI so that parsing logic is not duplicated across languages.
		//
		// This plugin never opens a *.diagamemessages file for writing and has zero
		// dependency on DiaMessageBus / Bus.h / DiaDebugServer / live game state.
		class DiaSchemaBrowserPlugin : public Dia::Editor::IEditorPlugin
		{
		public:
			const char* GetName()        const override { return "DiaSchemaBrowser"; }
			const char* GetVersion()     const override { return "0.1.0"; }
			const char* GetDescription() const override { return "Offline browser for .diagamemessages schema declarations (read-only)"; }
			const char* GetUIPath()      const override { return "dia://plugins/schemabrowser/index.html"; }
			Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

			void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

		private:
			void RegisterRequestHandlers();

			// Walk up from the running executable to locate the repo root
			// (the directory that contains pipeline.toml). Fills mRepoRoot.
			void ResolveRepoRoot();

			// Recursively collect every *.diagamemessages file under `dir`,
			// pruning heavy/noise directories. Read-only.
			void CollectSchemaFiles(const char* dir, Json::Value& outFiles) const;

			// Read + JSON-parse a single file (read-only). Returns true on success.
			bool ParseSchemaFile(const char* fullPath, Json::Value& outDoc) const;

			// Build the { documents:[...], fileCount, messageCount } scan result.
			Json::Value BuildScanResult() const;

			Dia::Editor::WebUIBridge* mBridge = nullptr;
			char mRepoRoot[1024];
		};
	}
}
