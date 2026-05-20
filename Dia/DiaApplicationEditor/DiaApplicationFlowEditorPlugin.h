#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/FilePath/FileWatcher.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationEditor/V2/Commands/CommandHistory.h>
#include <DiaApplicationEditor/V2/TypeDiscoveryService.h>
#include <DiaApplicationEditor/V2/LiveStateStore.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include "DiaApplicationEditor/EditorHealthReporter.h"

namespace Json { class Value; }

namespace Dia { namespace Editor {
    class WebUIBridge;
    class GameConnectionManager;
}}

namespace Dia { namespace Editor {

    class DiaApplicationFlowEditorPlugin : public IEditorPlugin
    {
    public:
        const char* GetName() const override        { return "Application Flow Editor"; }
        const char* GetVersion() const override     { return "2.0"; }
        const char* GetDescription() const override { return "Visual editor for .diaapp v2 manifests with live runtime inspection"; }
        const char* GetUIPath() const override      { return "dia://plugins/diaapplicationeditor/index.html"; }
        LayoutMode GetLayoutMode() const override   { return LayoutMode::kFullScreen; }

        EditorToolbarItem GetToolbarItem() const override;

        void OnLoad(const EditorPluginContext& context) override;
        void OnUnload() override;
        void OnUpdate(float deltaTime) override;

    private:
        Json::Value HandleManifestLoad(const Json::Value& data);
        Json::Value HandleManifestSave(const Json::Value& data);
        Json::Value HandleManifestGetState(const Json::Value& data);
        Json::Value HandleHistoryUndo(const Json::Value& data);
        Json::Value HandleHistoryRedo(const Json::Value& data);
        Json::Value HandleHistoryGetState(const Json::Value& data);
        Json::Value HandleValidationRun(const Json::Value& data);
        Json::Value HandleTypesGet(const Json::Value& data);
        Json::Value HandleTypesRefresh(const Json::Value& data);
        Json::Value HandleRiskCheck(const Json::Value& data);
        Json::Value HandleRiskConfirm(const Json::Value& data);
        Json::Value HandleLiveConnect(const Json::Value& data);
        Json::Value HandleLiveDisconnect(const Json::Value& data);
        Json::Value HandleLiveGetStatus(const Json::Value& data);
        Json::Value HandleLiveTransitionTo(const Json::Value& data);
        Json::Value HandleLiveShutdown(const Json::Value& data);

        WebUIBridge* mBridge = nullptr;
        GameConnectionManager* mGameConnection = nullptr;
        EditorModel* mModel = nullptr;

        Dia::ApplicationFlow::Editor::ManifestEditorState mEditorState;
        Dia::ApplicationFlow::Editor::CommandHistory mCommandHistory;
        Dia::ApplicationFlow::Editor::TypeDiscoveryService mTypeDiscovery;
        Dia::ApplicationFlow::Editor::LiveStateStore mLiveStore;

        Dia::Core::FileWatcher mFileWatcher;
        bool mSuppressFileWatchDuringSave = false;

        // Path stored so React can pull it via manifest.getState after mounting
        Dia::Core::Containers::String512 mPendingManifestPath;

        // Live connection state (used by health reporter via reference)
        bool mIsLiveConnected = false;

        // Health reporter (holds refs to mEditorState and mIsLiveConnected)
        EditorHealthReporter mHealthReporter{ mEditorState, mIsLiveConnected };

        // Metrics
        Dia::Observation::Metric::Gauge*   mMetricLoadMs             = nullptr;
        Dia::Observation::Metric::Gauge*   mMetricSaveMs             = nullptr;
        Dia::Observation::Metric::Gauge*   mMetricValidationErrors   = nullptr;
        Dia::Observation::Metric::Gauge*   mMetricValidationWarnings = nullptr;
        Dia::Observation::Metric::Gauge*   mMetricConnectionState    = nullptr;
        Dia::Observation::Metric::Counter* mMetricCommandsTotal      = nullptr;
    };

}} // namespace Dia::Editor
