#include "Modules/TestStages/TestStageHUDModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"
#include "Modules/AutomationModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <imgui.h>

namespace CluicheTest {

const Dia::Core::StringCRC TestStageHUDModule::kTypeId("TestStageHUDModule");

TestStageHUDModule::TestStageHUDModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult TestStageHUDModule::DoStart()
{
    if (!Cluiche::AppFlow::AutomationModule::GetStatic())
        return Dia::ApplicationFlow::StartResult::kLoading;
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageHUDModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("TestStageHUDModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive())
        return;

    auto* automationModule = Cluiche::AppFlow::AutomationModule::GetStatic();
    auto* svc = automationModule ? automationModule->GetService() : nullptr;
    if (svc && svc->IsHeartbeatEnabled())
        return;

    RenderBottomBar();
}

void TestStageHUDModule::RenderBottomBar()
{
    auto& registry = TestResultsRegistry::GetInstance();
    const Dia::Core::StringCRC activeStageName = registry.GetActiveStage();
    const StageResult* result = (activeStageName != Dia::Core::StringCRC())
        ? registry.GetResult(activeStageName) : nullptr;

    if (!result)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    const float barH = 28.0f;

    const bool isPassed  = result->state == StageResult::State::kPassed;
    const bool isTimeout = result->state == StageResult::State::kTimeout;

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

    // Stage name
    ImGui::Text("%s", activeStageName.AsChar());
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();

    // Checkpoint icons
    const auto& checkpoints = result->checkpoints;
    if (checkpoints.Size() == 0)
    {
        ImGui::TextDisabled("(no checkpoints)");
        ImGui::SameLine();
    }
    else
    {
        for (unsigned int i = 0; i < checkpoints.Size(); ++i)
        {
            if (isPassed)
            {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s %s", "\xe2\x9c\x93", checkpoints[i].AsChar());
            }
            else if (isTimeout)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s %s", "\xe2\x9c\x97", checkpoints[i].AsChar());
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "%s %s", "\xe2\x8f\xb3", checkpoints[i].AsChar());
            }
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
        }
    }

    // Frame counter
    const unsigned int frame  = registry.GetActiveFrameCount();
    const unsigned int budget = result->budgetFrames;
    if (isPassed)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PASS  %u/%u", frame, budget);
    else if (isTimeout)
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "TIMEOUT  %u/%u", frame, budget);
    else
        ImGui::Text("%u / %u", frame, budget);

    // Exit button — far right
    ImGui::SameLine(io.DisplaySize.x - 38.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f,  0.05f, 0.05f, 1.0f));
    if (ImGui::Button(" X "))
    {
        DIA_LOG_INFO("CluicheTest", "HUD exit clicked — stage '%s' state %d — navigating to Boot",
            activeStageName.AsChar(), static_cast<int>(result->state));
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

} // namespace CluicheTest

namespace { using TestStageHUDModule_ = CluicheTest::TestStageHUDModule; }
DIA_MODULE(TestStageHUDModule_);
