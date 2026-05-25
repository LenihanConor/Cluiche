#include "Modules/BootMenuModule.h"
#include "Modules/DebugUIModule.h"
#include "Modules/DebugServerHostModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaDebugServer/DebugServer.h>

#include <imgui.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC BootMenuModule::kTypeId("BootMenuModule");

BootMenuModule::BootMenuModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult BootMenuModule::DoStart()
{
    DIA_LOG_INFO("Application", "BootMenuModule DoStart entry");

    DebugUIModule* debugUI = mDebugUI.Get();
    if (debugUI == nullptr)
        return Dia::ApplicationFlow::StartResult::kLoading;

    CacheNavigableStages();

    mStageCountGauge = Dia::Observation::Metric::MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("boot_menu.stage_count"));
    mLaunchCounter = Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("boot_menu.launches"));
    mStageCountGauge->Set(static_cast<double>(mNavigableStages.Size()));

    // Mark stages as loaded if we're returning to Boot from a stage
    for (unsigned int i = 0; i < mNavigableStages.Size(); ++i)
    {
        if (CluicheTest::TestResultsRegistry::IsCreated())
        {
            const CluicheTest::StageResult* result =
                CluicheTest::TestResultsRegistry::GetInstance().GetResult(mNavigableStages[i]);
            if (result && result->state != CluicheTest::StageResult::State::kNotRun)
                MarkLoaded(i);
        }
    }

    DIA_LOG_INFO("Application", "BootMenuModule DoStart exit (%u stages)", mNavigableStages.Size());
    return Dia::ApplicationFlow::StartResult::kReady;
}

void BootMenuModule::DoUpdate(float /*dt*/)
{
    if (DebugUIModule* debugUI = mDebugUI.Get())
    {
        if (!debugUI->IsFrameActive())
            return;
    }
    DrawMenu();
}

Dia::ApplicationFlow::StopResult BootMenuModule::DoStop()
{
    DIA_LOG_INFO("Application", "BootMenuModule DoStop entry");
    DIA_LOG_INFO("Application", "BootMenuModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void BootMenuModule::CacheNavigableStages()
{
    mNavigableStages.RemoveAll();
    GetApplication()->GetStageTransitions(Dia::Core::StringCRC("Boot"), mNavigableStages);
}

void BootMenuModule::DrawMenu()
{
    DIA_TRACE_ZONE("BootMenu.DrawMenu", Dia::Observation::Trace::Category::kDiaApplicationFlow);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_Once);

    ImGui::Begin("CluicheTest \xe2\x80\x94 Bootstrap");

    // Header
    ImGui::Text("Project: CluicheTest v0.1");
    ImGui::SameLine();

    bool automationConnected = false;
    if (DebugServerHostModule* dsm = mDebugServer.Get())
    {
        if (Dia::DebugServer::DebugServer* server = dsm->GetServer())
            automationConnected = server->GetConnectionCount() > 0;
    }

    if (automationConnected)
    {
        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextColored(ImVec4(0.29f, 0.73f, 0.88f, 1.0f), "Automation: Connected");
    }

    ImGui::TextDisabled("%u stages available", mNavigableStages.Size());
    ImGui::Separator();

    // Stage table
    if (ImGui::BeginTable("##stages", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
    {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (unsigned int i = 0; i < mNavigableStages.Size(); ++i)
        {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            // Loaded indicator
            ImGui::TableSetColumnIndex(0);
            if (IsLoadedThisSession(i))
                ImGui::TextColored(ImVec4(0.29f, 0.88f, 0.29f, 1.0f), "\xe2\x97\x8f");
            else
                ImGui::TextDisabled("\xe2\x97\x8b");

            // Stage name (selectable row)
            ImGui::TableSetColumnIndex(1);
            bool isSelected = (mSelectedIndex == static_cast<int>(i));
            if (ImGui::Selectable(mNavigableStages[i].AsChar(), isSelected,
                    ImGuiSelectableFlags_SpanAllColumns))
            {
                mSelectedIndex = static_cast<int>(i);
            }

            // Status badge
            ImGui::TableSetColumnIndex(2);
            const char* status = GetStatusLabel(mNavigableStages[i]);
            if (strcmp(status, "PASSED") == 0)
                ImGui::TextColored(ImVec4(0.29f, 0.88f, 0.29f, 1.0f), "%s", status);
            else if (strcmp(status, "FAILED") == 0 || strcmp(status, "TIMEOUT") == 0)
                ImGui::TextColored(ImVec4(0.88f, 0.29f, 0.29f, 1.0f), "%s", status);
            else
                ImGui::TextDisabled("%s", status);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    // Launch button
    bool canLaunch = mSelectedIndex >= 0 &&
        mSelectedIndex < static_cast<int>(mNavigableStages.Size());

    if (!canLaunch)
        ImGui::BeginDisabled();

    if (ImGui::Button("Launch") || (canLaunch && ImGui::IsKeyPressed(ImGuiKey_Enter)))
    {
        const Dia::Core::StringCRC& target = mNavigableStages[mSelectedIndex];
        DIA_LOG_INFO("Application", "BootMenuModule: Launch -> TransitionTo('%s')", target.AsChar());
        mLaunchCounter->Inc();
        TransitionTo(target);
    }

    if (!canLaunch)
        ImGui::EndDisabled();

    // Session stats
    unsigned int passed = 0, failed = 0, pending = 0;
    if (CluicheTest::TestResultsRegistry::IsCreated())
    {
        const auto& registry = CluicheTest::TestResultsRegistry::GetInstance();
        for (unsigned int i = 0; i < mNavigableStages.Size(); ++i)
        {
            const CluicheTest::StageResult* result = registry.GetResult(mNavigableStages[i]);
            if (!result || result->state == CluicheTest::StageResult::State::kNotRun)
                ++pending;
            else if (result->state == CluicheTest::StageResult::State::kPassed)
                ++passed;
            else
                ++failed;
        }
    }
    else
    {
        pending = mNavigableStages.Size();
    }

    ImGui::SameLine(0.0f, 24.0f);
    ImGui::TextColored(ImVec4(0.29f, 0.88f, 0.29f, 1.0f), "Passed: %u", passed);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.88f, 0.29f, 0.29f, 1.0f), "Failed: %u", failed);
    ImGui::SameLine();
    ImGui::TextDisabled("Pending: %u", pending);

    // Footer
    ImGui::Separator();
    ImGui::TextDisabled("Enter = Launch");

    ImGui::End();
}

const char* BootMenuModule::GetStatusLabel(const Dia::Core::StringCRC& stageCrc) const
{
    if (!CluicheTest::TestResultsRegistry::IsCreated())
        return "NOT RUN";

    const CluicheTest::StageResult* result =
        CluicheTest::TestResultsRegistry::GetInstance().GetResult(stageCrc);
    if (!result)
        return "NOT RUN";

    switch (result->state)
    {
    case CluicheTest::StageResult::State::kPassed:  return "PASSED";
    case CluicheTest::StageResult::State::kFailed:  return "FAILED";
    case CluicheTest::StageResult::State::kTimeout: return "TIMEOUT";
    case CluicheTest::StageResult::State::kRunning: return "RUNNING";
    default:                                        return "NOT RUN";
    }
}

bool BootMenuModule::IsLoadedThisSession(unsigned int index) const
{
    return (mLoadedBitfield & (1u << index)) != 0;
}

void BootMenuModule::MarkLoaded(unsigned int index)
{
    mLoadedBitfield |= (1u << index);
}

} } // namespace Cluiche::AppFlow

namespace { using BootMenuModule_ = Cluiche::AppFlow::BootMenuModule; }
DIA_MODULE(BootMenuModule_);
