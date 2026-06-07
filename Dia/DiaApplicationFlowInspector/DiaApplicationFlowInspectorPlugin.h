#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaApplicationFlowInspector/LiveStateStore.h>
#include "DiaApplicationFlowInspector/InspectorHealthReporter.h"
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Json { class Value; }

namespace Dia { namespace Editor {
    class GameConnectionManager;
}}

namespace Dia { namespace Editor {

    class DiaApplicationFlowInspectorPlugin : public EditorPluginBase
    {
    public:
        DiaApplicationFlowInspectorPlugin();

        void OnPluginLoad() override;
        void OnPluginUnload() override;
        void OnUpdate(float deltaTime) override;

    private:
        Json::Value HandleLiveConnect(const Json::Value& data);
        Json::Value HandleLiveDisconnect(const Json::Value& data);
        Json::Value HandleLiveGetStatus(const Json::Value& data);
        Json::Value HandleLiveTransitionTo(const Json::Value& data);
        Json::Value HandleLiveShutdown(const Json::Value& data);

        GameConnectionManager* mGameConnection = nullptr;
        Dia::ApplicationFlow::Editor::LiveStateStore mLiveStore;
        bool mIsLiveConnected = false;
        float mSecondsSinceLastData = 999.0f;
        InspectorHealthReporter mHealthReporter{ mIsLiveConnected, mSecondsSinceLastData };
        Dia::Observation::Metric::Counter* mMetricEventsTotal = nullptr;
    };

}} // namespace Dia::Editor
