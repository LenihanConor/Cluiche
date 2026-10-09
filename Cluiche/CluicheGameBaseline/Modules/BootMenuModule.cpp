#include "Modules/BootMenuModule.h"
#include "Modules/DebugUIModule.h"
#include "Modules/DebugServerHostModule.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaCore/Json/external/json/json.h>

#include <imgui.h>
#include <cstring>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC BootMenuModule::kTypeId("BootMenuModule");

BootMenuModule::BootMenuModule(const Dia::Core::StringCRC& instanceId)
    : RenderModule(instanceId)
{
    strncpy_s(mTitle, BootMenuLogic::ResolveTitle(nullptr), _TRUNCATE);
    strncpy_s(mProject, BootMenuLogic::ResolveProject(nullptr), _TRUNCATE);
}

void BootMenuModule::OnConfigure(const char* configJson)
{
    if (configJson == nullptr || configJson[0] == '\0')
        return;

    Json::Value config;
    Json::Reader reader;
    if (!reader.parse(configJson, config))
    {
        DIA_LOG_WARNING("Application", "BootMenuModule: OnConfigure - JSON parse failed");
        return;
    }

    const char* title = (config.isMember("title") && config["title"].isString()) ? config["title"].asCString() : nullptr;
    const char* project = (config.isMember("project") && config["project"].isString()) ? config["project"].asCString() : nullptr;
    strncpy_s(mTitle, BootMenuLogic::ResolveTitle(title), _TRUNCATE);
    strncpy_s(mProject, BootMenuLogic::ResolveProject(project), _TRUNCATE);
}

void BootMenuModule::RefreshProvider()
{
    // ModuleRef::Get() returns null unless the target module is active.
    mLogic.SetProvider(mStageStatus.Get());
}

Dia::ApplicationFlow::StartResult BootMenuModule::DoStart()
{
    DebugUIModule* debugUI = mDebugUI.Get();
    if (debugUI == nullptr)
        return Dia::ApplicationFlow::StartResult::kLoading;

    DIA_LOG_INFO("Application", "BootMenuModule DoStart entry");

    CacheNavigableStages();

    mStageCountGauge = Dia::Observation::Metric::MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("boot_menu.stage_count"));
    mLaunchCounter = Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("boot_menu.launches"));
    mStageCountGauge->Set(static_cast<double>(mNavigableStages.Size()));

    // Returning to Boot from a stage: restore visited markers from the status provider (if any).
    mSelectedIndex = -1;
    RefreshProvider();
    mLogic.RestoreVisited();

    DIA_LOG_INFO("Application", "BootMenuModule DoStart exit (%u stages)", mNavigableStages.Size());
    return Dia::ApplicationFlow::StartResult::kReady;
}

void BootMenuModule::DoUpdate(const Dia::SimTime::RenderTimeContext& /*ctx*/)
{
    if (DebugUIModule* debugUI = mDebugUI.Get())
    {
        const bool frameActive = debugUI->IsFrameActive();
        if (!frameActive)
        {
            if (mFrameWasActive)
            {
                DIA_LOG_INFO("Application", "BootMenuModule: frame became inactive");
                mFrameWasActive = false;
            }
            return;
        }
        if (!mFrameWasActive)
        {
            DIA_LOG_INFO("Application", "BootMenuModule: frame became active");
            mFrameWasActive = true;
        }
    }
    RefreshProvider();
    DrawMenu();
}

Dia::ApplicationFlow::StopResult BootMenuModule::DoStop()
{
    DIA_LOG_INFO("Application", "BootMenuModule DoStop entry");
    mSelectedIndex = -1;
    mLogic.SetProvider(nullptr);
    DIA_LOG_INFO("Application", "BootMenuModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void BootMenuModule::CacheNavigableStages()
{
    mNavigableStages.RemoveAll();
    GetApplication()->GetStageTransitions(Dia::Core::StringCRC("Boot"), mNavigableStages);

    Dia::Core::StringCRC stages[kMaxStages];
    for (unsigned int i = 0; i < mNavigableStages.Size(); ++i)
        stages[i] = mNavigableStages[i];
    mLogic.SetStages(stages, mNavigableStages.Size());
}

void BootMenuModule::DrawMenu()
{
    DIA_TRACE_ZONE("BootMenu.DrawMenu", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (mFirstDraw)
    {
        DIA_LOG_INFO("Application", "BootMenuModule: first draw (%u stages)", mNavigableStages.Size());
        mFirstDraw = false;
    }

    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_Once);

    ImGui::Begin(mTitle);

    // Header
    if (mProject[0] != '\0')
    {
        ImGui::Text("%s", mProject);
        ImGui::SameLine();
    }

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

    // Stage table (badge column only when a StageStatusModule is present)
    const bool hasStatus = mLogic.HasStatus();
    if (ImGui::BeginTable("##stages", hasStatus ? 3 : 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
    {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch);
        if (hasStatus)
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (unsigned int i = 0; i < mNavigableStages.Size(); ++i)
        {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            // Visited indicator
            ImGui::TableSetColumnIndex(0);
            if (mLogic.IsVisited(mNavigableStages[i]))
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
                DIA_LOG_INFO("Application", "BootMenuModule: selected stage[%u] '%s'",
                             i, mNavigableStages[i].AsChar());
            }

            // Status badge
            if (hasStatus)
            {
                ImGui::TableSetColumnIndex(2);
                BootMenuLogic::Badge badge;
                if (mLogic.GetBadge(mNavigableStages[i], badge) && badge.label != nullptr)
                {
                    switch (badge.severity)
                    {
                    case StageStatusModule::Severity::kOk:
                        ImGui::TextColored(ImVec4(0.29f, 0.88f, 0.29f, 1.0f), "%s", badge.label);
                        break;
                    case StageStatusModule::Severity::kFail:
                        ImGui::TextColored(ImVec4(0.88f, 0.29f, 0.29f, 1.0f), "%s", badge.label);
                        break;
                    case StageStatusModule::Severity::kWarn:
                        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "%s", badge.label);
                        break;
                    default:
                        ImGui::TextDisabled("%s", badge.label);
                        break;
                    }
                }
                else
                {
                    ImGui::TextDisabled("-");
                }
            }

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
        DIA_LOG_INFO("Application", "BootMenuModule: Launch button -> navigate_to('%s')", target.AsChar());
        mLaunchCounter->Inc();
        mLogic.MarkVisited(target);
        mNavRequest.Send({target});
    }

    if (!canLaunch)
        ImGui::EndDisabled();

    // Counts footer (only when a StageStatusModule exists)
    if (hasStatus)
    {
        const BootMenuLogic::Counts counts = mLogic.CountBySeverity();
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::TextColored(ImVec4(0.29f, 0.88f, 0.29f, 1.0f), "Passed: %u", counts.passed);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.88f, 0.29f, 0.29f, 1.0f), "Failed: %u", counts.failed);
        ImGui::SameLine();
        ImGui::TextDisabled("Pending: %u", counts.pending);
    }

    // Footer
    ImGui::Separator();
    ImGui::TextDisabled("Enter = Launch");

    ImGui::End();
}

void BootMenuModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mNavRequest.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using BootMenuModule_ = Cluiche::AppFlow::BootMenuModule; }
DIA_MODULE(BootMenuModule_);
DIA_DESCRIBE(BootMenuModule_::kTypeId, "Displays a boot menu for selecting which stage to launch, with optional per-stage status badges.");
