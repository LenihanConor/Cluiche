#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include "DiaApplicationEditor/ManifestEditorData.h"
#include <DiaCore/FilePath/FileWatcher.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Application
	{
		namespace Editor
		{
			class DiaApplicationEditor : public Dia::Editor::IEditorPlugin
			{
			public:
				DiaApplicationEditor();
				~DiaApplicationEditor();

				// IEditorPlugin
				const char* GetName() const override { return "DiaApplicationEditor"; }
				const char* GetVersion() const override { return "1.0.0"; }
				const char* GetDescription() const override { return "Visual editor for .diaapp manifest files"; }
				const char* GetUIPath() const override { return "dia://plugins/diaapplicationeditor/index.html"; }
				Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

				void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
				void OnUnload() override;
				void OnUpdate(float deltaTime) override;

				// File operations
				void OpenManifest(const char* path);
				void SaveManifest();
				void SaveManifestAs(const char* path);
				void CloseManifest();

				// Dirty tracking
				bool IsDirty() const;
				void MarkDirty();
				void MarkClean();

				// Validation
				bool ValidateManifest();
				const ValidationResult& GetValidationResult() const;

				// Type discovery — offline static list; live query wired in Phase 3
				void RefreshAvailableTypes();
				const TypeCache& GetTypeCache() const;

				// Data access
				const ManifestEditorData* GetData() const { return mData; }

			private:
				ManifestEditorData* mData;
				Dia::Editor::WebUIBridge* mBridge;
				Dia::Core::FileWatcher mFileWatcher;
				bool mIsSaving;

				bool WriteManifestToDisk(const char* path);
				bool WriteDiaGameToDisk(const char* path);
				void NotifyUI(const char* topic, const char* message = nullptr);
				void NotifyManifestUpdated();
				void LoadStaticTypeList();

				// Tree node mutation handlers (registered as bridge event handlers)
				void HandleAddNode(const Json::Value& data);
				void HandleAddNodeConfirmed(const Json::Value& data);
				void HandleRemoveNode(const Json::Value& data);
				void HandleReorderNode(const Json::Value& data);

				// Flow view / transition handlers
				void HandlePhasePositionsChanged(const Json::Value& data);
				void HandleTransitionAdded(const Json::Value& data);
				void HandleTransitionRemoved(const Json::Value& data);
				void HandleTransitionConditionChanged(const Json::Value& data);

				// Module config editor handler
				void HandleModuleConfigChanged(const Json::Value& data);

				// File conflict resolution handler
				void HandleResolveConflict(const Json::Value& data);

				// PU/Phase property handlers
				void HandlePUPropertyChanged(const Json::Value& data);
				void HandlePhasePropertyChanged(const Json::Value& data);
				void HandleInitialPhaseChanged(const Json::Value& data);

				// Module phase association handler
				void HandleModulePhasesChanged(const Json::Value& data);

				// New manifest / undo-restore handlers
				void HandleNewManifest();
				void HandleManifestRestored(const Json::Value& data);

				// Import management handlers
				void HandleAddImport(const Json::Value& data);
				void HandleRemoveImport(const Json::Value& data);

				// File watcher helpers
				void StartWatchingFile();
				void StopWatchingFile();

				// Node ID lookup helpers: "puId_phaseId" or "puId_phaseId_moduleId"
				int FindProcessingUnitIndex(const char* puId) const;
				int FindPhaseIndex(int puIndex, const char* phaseId) const;
				int FindModuleIndex(int puIndex, const char* moduleId) const;
			};
		}
	}
}
