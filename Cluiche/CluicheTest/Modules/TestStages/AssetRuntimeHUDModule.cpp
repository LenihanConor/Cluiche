#include "Modules/TestStages/AssetRuntimeHUDModule.h"
#include "Modules/AssetServiceModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
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

    auto* svc = Cluiche::AppFlow::AssetServiceModule::GetStatic();

    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 380.0f), ImGuiCond_Once);
    ImGui::Begin("Asset Runtime");

    // Stage load state section — use atomic-safe API only (no GetRuntime() cross-PU)
    if (ImGui::CollapsingHeader("Stage Load State", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (svc)
        {
            const bool complete = svc->IsStageLoadComplete(Dia::Core::StringCRC("AssetRuntimeStage"));
            ImGui::Text("Complete:"); ImGui::SameLine();
            if (complete)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "YES");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "loading...");
        }
        else
        {
            ImGui::TextDisabled("AssetServiceModule not ready");
        }
    }

    // Asset rows — state derived from stage-level load complete (atomic-safe)
    if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        struct AssetEntry { const char* id; const char* typeLabel; };
        static const AssetEntry kAssets[] = {
            { "texture.ar_tex1", "[T]" },
            { "texture.ar_tex2", "[T]" },
            { "texture.ar_tex3", "[T]" },
            { "json.ar_config",  "{}" },
        };

        const bool stageComplete = svc && svc->IsStageLoadComplete(Dia::Core::StringCRC("AssetRuntimeStage"));

        for (const auto& entry : kAssets)
        {
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 1.0f), "%s", entry.typeLabel);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", entry.id);
            ImGui::SameLine();
            if (stageComplete)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "LOADED");
            else
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "staged");
        }
    }

    // Checkpoints section
    if (framePtr && ImGui::CollapsingHeader("Checkpoints", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const Cluiche::AppFlow::StageHUDState& hud = framePtr->stageHUD;
        using HUDState = Cluiche::AppFlow::StageHUDState::StageState;
        const bool isPassed = (hud.stageState == HUDState::kPassed);

        for (unsigned int i = 0; i < hud.checkpointCount && i < Cluiche::AppFlow::StageHUDState::kMaxCheckpoints; ++i)
        {
            if (isPassed)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[PASS] %s", hud.checkpoints[i].AsChar());
            else
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "[...] %s", hud.checkpoints[i].AsChar());
        }
    }

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
