#pragma once

#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include <DiaApplicationFlowInspector/LiveStateStore.h>
#include "DiaApplicationFlowInspector/InspectorHealthReporter.h"
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Json { class Value; }

namespace Dia { namespace Editor {

    class DiaApplicationFlowInspectorPlugin : public LiveConnectionPluginBase
    {
    public:
        DiaApplicationFlowInspectorPlugin();

    protected:
        void OnLivePluginLoad() override;
        void OnLivePluginUnload() override;
        void OnGameConnected() override;
        void OnGameDisconnected() override;

    private:
        Json::Value HandleLiveConnect(const Json::Value& data);
        Json::Value HandleLiveDisconnect(const Json::Value& data);
        Json::Value HandleLiveGetStatus(const Json::Value& data);
        Json::Value HandleLiveTransitionTo(const Json::Value& data);
        Json::Value HandleLiveShutdown(const Json::Value& data);

        Dia::ApplicationFlow::Editor::LiveStateStore mLiveStore;
        bool mIsLiveConnected = false;
        float mSecondsSinceLastData = 999.0f;
        InspectorHealthReporter mHealthReporter{ mIsLiveConnected, mSecondsSinceLastData };
        Dia::Observation::Metric::Counter* mMetricEventsTotal = nullptr;
    };

}} // namespace Dia::Editor
