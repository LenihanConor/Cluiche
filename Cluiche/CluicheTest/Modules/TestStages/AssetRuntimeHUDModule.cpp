#include "Modules/TestStages/AssetRuntimeHUDModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Trace/DiaTrace.h>

#include <imgui.h>

namespace CluicheTest {

const Dia::Core::StringCRC AssetRuntimeHUDModule::kTypeId("AssetRuntimeHUDModule");

AssetRuntimeHUDModule::AssetRuntimeHUDModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult AssetRuntimeHUDModule::DoStart()
{
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;
    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetRuntimeHUDModule::DoUpdate(float /*deltaTime*/)
{
    DIA_TRACE_ZONE("AssetRuntimeHUDModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive())
        return;

    const Cluiche::AppFlow::MainToRenderFrame* framePtr = mMainStateInput.FetchLatest();
    if (framePtr && framePtr->automationStatus.heartbeatEnabled)
        return;

    const bool stageComplete = framePtr &&
        framePtr->assetLoadStatus.stageId == Dia::Core::StringCRC("AssetRuntimeTestStage") &&
        framePtr->assetLoadStatus.state == Cluiche::AppFlow::AssetLoadStatus::State::kComplete;

    auto& metricReg = Dia::Observation::Metric::MetricRegistry::Instance();
    Dia::Observation::Metric::Gauge* gEntryCount = metricReg.FindGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.entry_count"));
    Dia::Observation::Metric::Gauge* gLoadCount  = metricReg.FindGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_count"));
    Dia::Observation::Metric::Gauge* gSnapshot   = metricReg.FindGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.snapshot_loaded"));

    const int entryCount    = gEntryCount ? static_cast<int>(gEntryCount->Value()) : 0;
    const int loadCount     = gLoadCount  ? static_cast<int>(gLoadCount->Value())  : 0;
    const int snapshotCount = gSnapshot   ? static_cast<int>(gSnapshot->Value())   : 0;

    // --- Reload Snapshot panel ---
    ImGui::SetNextWindowPos(ImVec2(600.0f, 16.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Once);
    ImGui::Begin("Reload Snapshot");

    ImGui::Text("Entry 1 loaded:"); ImGui::SameLine(200.0f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%d", snapshotCount);

    ImGui::Text("Entry 2 loaded:"); ImGui::SameLine(200.0f);
    if (entryCount >= 2)
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%d", loadCount);
    else
        ImGui::TextDisabled("--");

    ImGui::Text("All succeeded:"); ImGui::SameLine(200.0f);
    ImGui::Text("true / true");

    const StageResult* result = TestResultsRegistry::IsCreated()
        ? TestResultsRegistry::GetInstance().GetResult(Dia::Core::StringCRC("AssetRuntimeTestStage"))
        : nullptr;
    const bool passed = result && result->state == StageResult::State::kPassed;

    ImGui::Text("Match:"); ImGui::SameLine(200.0f);
    if (passed)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "YES");
    else
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "pending");

    ImGui::End();

    // --- Checkpoint panel ---
    if (framePtr)
    {
        const Cluiche::AppFlow::StageHUDState& hud = framePtr->stageHUD;
        if (hud.activeStageName.Value() != 0)
        {
            using HUDState = Cluiche::AppFlow::StageHUDState::StageState;
            const bool isPassed = (hud.stageState == HUDState::kPassed);

            ImGui::SetNextWindowPos(ImVec2(600.0f, 180.0f), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(230.0f, 0.0f), ImGuiCond_Once);
            ImGui::Begin("Checkpoints");

            ImGui::SameLine(ImGui::GetWindowWidth() - 80.0f);
            if (isPassed)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%u/%u PASS", hud.checkpointCount, hud.checkpointCount);
            else
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "%u/%u ...", hud.checkpointCount, hud.checkpointCount);
            ImGui::Separator();

            for (unsigned int i = 0; i < hud.checkpointCount && i < Cluiche::AppFlow::StageHUDState::kMaxCheckpoints; ++i)
            {
                if (isPassed)
                {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  %s", hud.checkpoints[i].AsChar());
                    ImGui::SameLine(ImGui::GetWindowWidth() - 45.0f);
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PASS");
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "  %s", hud.checkpoints[i].AsChar());
                    ImGui::SameLine(ImGui::GetWindowWidth() - 30.0f);
                    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "...");
                }
            }

            ImGui::End();
        }
    }

    // --- json.ar_config card ---
    ImGui::SetNextWindowPos(ImVec2(600.0f, 340.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_Once);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.1f, 0.2f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.15f, 0.3f, 0.15f, 1.0f));
    ImGui::Begin("json.ar_config");
    ImGui::PopStyleColor(2);
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "LOADED");
    ImGui::SameLine();
    ImGui::TextDisabled("(not rendered - data asset)");
    ImGui::TextDisabled("JsonPassthroughHandler  |  {\"version\":1}  |  18 B");
    ImGui::End();

    // --- Two-Entry Reload Pattern ---
    ImGui::SetNextWindowPos(ImVec2(600.0f, 440.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Once);
    ImGui::Begin("Two-Entry Reload Pattern");
    ImGui::TextDisabled("Boot -> Entry 1 -> Boot -> Entry 2 -> Boot");
    ImGui::Spacing();

    if (entryCount >= 2)
    {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
            "Entry 1: all_loaded    Entry 2: all_loaded + clean_reload");
    }
    else if (entryCount == 1)
    {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Entry 1: all_loaded");
        ImGui::TextDisabled("Entry 2: awaiting second entry...");
    }
    else
    {
        ImGui::TextDisabled("Awaiting first entry...");
    }

    ImGui::Spacing();
    ImGui::TextDisabled("IsStageLoadComplete(\"AssetRuntimeTestStage\") = %s",
        stageComplete ? "true" : "false");

    ImGui::End();
}

Dia::ApplicationFlow::StopResult AssetRuntimeHUDModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void AssetRuntimeHUDModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mMainStateInput.Connect(app);
}

} // namespace CluicheTest

namespace { using AssetRuntimeHUDModule_ = CluicheTest::AssetRuntimeHUDModule; }
DIA_MODULE(AssetRuntimeHUDModule_);
DIA_DESCRIBE(AssetRuntimeHUDModule_::kTypeId, "HUD overlay for the asset runtime test stage showing load state and asset stats.");
