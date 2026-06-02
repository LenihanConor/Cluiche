#include "Modules/TestStages/TestStageHUDModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/CRC/StringCRC.h>

#include <imgui.h>

namespace CluicheTest {

const Dia::Core::StringCRC TestStageHUDModule::kTypeId("TestStageHUDModule");

TestStageHUDModule::TestStageHUDModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult TestStageHUDModule::DoStart()
{
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageHUDModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("TestStageHUDModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive())
        return;

    // Fetch latest MainToRenderFrame from the stream.
    const Cluiche::AppFlow::MainToRenderFrame* frame = mMainStateInput.FetchLatest();
    if (!frame)
        return;

    // If automation heartbeat is enabled, skip HUD rendering.
    if (frame->automationStatus.heartbeatEnabled)
        return;

    RenderBottomBar(*frame);
}

void TestStageHUDModule::RenderBottomBar(const Cluiche::AppFlow::MainToRenderFrame& frame)
{
    const Cluiche::AppFlow::StageHUDState& hud = frame.stageHUD;

    if (hud.activeStageName.Value() == 0)
        return;

    using HUDState = Cluiche::AppFlow::StageHUDState::StageState;

    const ImGuiIO& io = ImGui::GetIO();
    const float barH = 28.0f;

    const bool isPassed  = (hud.stageState == HUDState::kPassed);
    const bool isTimeout = (hud.stageState == HUDState::kTimeout);

    if (isPassed)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.35f, 0.0f, 0.88f));
    else if (isTimeout)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.45f, 0.0f, 0.0f, 0.88f));
    else
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 0.85f));

    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - barH));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barH));

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##HUDBar", nullptr, kFlags);
    ImGui::PopStyleColor();

    // Spinner while running
    if (!isPassed && !isTimeout)
    {
        static const char* kSpinnerFrames[] = { "|", "/", "-", "\\" };
        unsigned int spinIdx = (hud.frameCount / 4) % 4;
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", kSpinnerFrames[spinIdx]);
        ImGui::SameLine();
    }

    // Stage name
    ImGui::Text("%s", hud.activeStageName.AsChar());
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();

    // Checkpoint icons
    if (hud.checkpointCount == 0)
    {
        ImGui::TextDisabled("(no checkpoints)");
        ImGui::SameLine();
    }
    else
    {
        for (unsigned int i = 0; i < hud.checkpointCount && i < Cluiche::AppFlow::StageHUDState::kMaxCheckpoints; ++i)
        {
            if (isPassed)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s %s", "\xe2\x9c\x93", hud.checkpoints[i].AsChar());
            else if (isTimeout)
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s %s", "\xe2\x9c\x97", hud.checkpoints[i].AsChar());
            else
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "%s %s", "\xe2\x8f\xb3", hud.checkpoints[i].AsChar());
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
        }
    }

    // Frame counter
    const unsigned int frame_  = hud.frameCount;
    const unsigned int budget  = hud.budgetFrames;
    if (isPassed)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PASS  %u/%u", frame_, budget);
    else if (isTimeout)
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "TIMEOUT  %u/%u", frame_, budget);
    else
        ImGui::Text("%u / %u", frame_, budget);

    // Exit button — far right
    ImGui::SameLine(io.DisplaySize.x - 38.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f,  0.05f, 0.05f, 1.0f));
    if (ImGui::Button(" X "))
    {
        DIA_LOG_INFO("CluicheTest", "HUD exit clicked — stage '%s' state %d — navigating to Boot",
            hud.activeStageName.AsChar(), static_cast<int>(hud.stageState));
        Json::Value params;
        params["target"] = "Boot";
        Dia::API::ExecuteCommandJson(Dia::Core::StringCRC("dia.automation.navigate_to"), params);
    }
    ImGui::PopStyleColor(3);

    ImGui::End();
}

Dia::ApplicationFlow::StopResult TestStageHUDModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void TestStageHUDModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mMainStateInput.Connect(app);
}

} // namespace CluicheTest

namespace { using TestStageHUDModule_ = CluicheTest::TestStageHUDModule; }
DIA_MODULE(TestStageHUDModule_);
DIA_DESCRIBE(TestStageHUDModule_::kTypeId, "Shared HUD overlay for all CluicheTest stages: shows stage name, controls, and test status.");
