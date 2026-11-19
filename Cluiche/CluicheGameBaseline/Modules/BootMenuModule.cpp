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
#include <cstdio>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC BootMenuModule::kTypeId("BootMenuModule");

namespace {

constexpr float kNameIndent = 22.0f;
constexpr float kPresetIndent = 24.0f;

ImVec4 BadgeColourVec(BootMenuLogic::BadgeColour colour)
{
    const unsigned int rgba = BootMenuLogic::BadgeColourRgba(colour);
    return ImVec4(
        static_cast<float>((rgba >> 24) & 0xFF) / 255.0f,
        static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgba & 0xFF) / 255.0f);
}

// Copies text into out, trimming with "..." until it fits maxWidth.
void FitText(const char* text, float maxWidth, char* out, size_t outSize)
{
    strncpy_s(out, outSize, text, _TRUNCATE);
    if (maxWidth <= 0.0f || ImGui::CalcTextSize(out).x <= maxWidth)
        return;
    size_t len = strlen(out);
    while (len > 0)
    {
        --len;
        out[len] = '\0';
        char probe[256];
        snprintf(probe, sizeof(probe), "%s...", out);
        if (ImGui::CalcTextSize(probe).x <= maxWidth)
        {
            strncpy_s(out, outSize, probe, _TRUNCATE);
            return;
        }
    }
    strncpy_s(out, outSize, "...", _TRUNCATE);
}

} // namespace

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

Dia::ApplicationFlow::StartResult BootMenuModule::DoStart()
{
    DebugUIModule* debugUI = mDebugUI.Get();
    if (debugUI == nullptr)
        return Dia::ApplicationFlow::StartResult::kLoading;

    DIA_LOG_INFO("Application", "BootMenuModule DoStart entry");

    mStageCountGauge = Dia::Observation::Metric::MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("boot_menu.stage_count"));
    mLaunchCounter = Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("boot_menu.launches"));

    // Returning to Boot from a stage: re-apply the latest snapshot (visited/collapse/filter state is kept by the logic).
    mWaitedSeconds = 0.0f;
    mHaveRevision = false;
    mFallbackLogged = false;
    mSelectedPreset = Dia::Core::StringCRC();

    DIA_LOG_INFO("Application", "BootMenuModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void BootMenuModule::PollSnapshot(float frameDt)
{
    const StageCatalogueSnapshot* snapshot = mSnapshotIn.FetchLatest();
    if (snapshot != nullptr)
    {
        if (!mHaveRevision || snapshot->revision != mLastRevision || mLogic.IsFallback())
        {
            mLogic.SetSnapshot(*snapshot);
            mLastRevision = snapshot->revision;
            mHaveRevision = true;
            OnRebuilt("snapshot");
        }
        return;
    }

    if (!mLogic.IsFallback() && !mLogic.HasSnapshot())
    {
        mWaitedSeconds += frameDt;
        if (mWaitedSeconds >= kFallbackSeconds)
            UseFallback();
    }
}

void BootMenuModule::UseFallback()
{
    mNavigableStages.RemoveAll();
    GetApplication()->GetStageTransitions(Dia::Core::StringCRC("Boot"), mNavigableStages);

    Dia::Core::StringCRC stages[kMaxStages];
    for (unsigned int i = 0; i < mNavigableStages.Size(); ++i)
        stages[i] = mNavigableStages[i];
    mLogic.SetFallbackStages(stages, mNavigableStages.Size());

    if (!mFallbackLogged)
    {
        DIA_LOG_WARNING("Application", "BootMenuModule: no StageCatalogueSnapshot within %.1f s, using the Boot transitions fallback (%u stages)",
                        static_cast<double>(kFallbackSeconds), mNavigableStages.Size());
        mFallbackLogged = true;
    }
    OnRebuilt("fallback");
}

void BootMenuModule::OnRebuilt(const char* why)
{
    if (mStageCountGauge != nullptr)
        mStageCountGauge->Set(static_cast<double>(mLogic.StageCount()));

    unsigned int rows = 0;
    for (unsigned int g = 0; g < mLogic.GroupCount(); ++g)
        rows += mLogic.RowCount(g);
    DIA_LOG_INFO("Application", "BootMenuModule: menu built from %s (%u stages, %u rows, %u groups)",
                 why, mLogic.StageCount(), rows, mLogic.GroupCount());
}

void BootMenuModule::DoUpdate(const Dia::SimTime::RenderTimeContext& ctx)
{
    PollSnapshot(ctx.frameDt);

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
    DrawMenu();
}

Dia::ApplicationFlow::StopResult BootMenuModule::DoStop()
{
    DIA_LOG_INFO("Application", "BootMenuModule DoStop entry");
    mLogic.ClearSelection();
    mSelectedPreset = Dia::Core::StringCRC();
    DIA_LOG_INFO("Application", "BootMenuModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void BootMenuModule::Launch(const Dia::Core::StringCRC& stage, const Dia::Core::StringCRC& preset, StageGroup fromGroup)
{
    mLogic.Launch(stage);
    mLaunchCounter->Inc();
    DIA_LOG_INFO("Application", "BootMenuModule: launch stage '%s' preset '%s' from group '%s'",
                 stage.AsChar(), preset.AsChar(), StageGroupName(fromGroup));
    mNavRequest.Send({stage, preset});
}

void BootMenuModule::DrawFilterBar()
{
    if (mLogic.IsFallback())
        return;

    // Chips (toggle)
    const float rightEdge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (unsigned int i = 0; i < mLogic.ChipCount(); ++i)
    {
        const bool active = mLogic.IsChipActive(i);
        const char* name = mLogic.ChipName(i);
        const float w = ImGui::CalcTextSize(name).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (i > 0)
        {
            if (ImGui::GetCursorScreenPos().x + ImGui::GetStyle().ItemSpacing.x + w < rightEdge)
                ImGui::SameLine();
        }
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.65f, 1.0f));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton(name))
        {
            mLogic.ToggleChip(i);
            DIA_LOG_DEBUG("Application", "BootMenuModule: chip '%s' -> %s", name, mLogic.IsChipActive(i) ? "on" : "off");
            OnRebuilt("filter");
        }
        ImGui::PopID();
        if (active)
            ImGui::PopStyleColor();
    }

    // Search
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::InputTextWithHint("##search", "Search stages...", mSearchBuf, sizeof(mSearchBuf)))
    {
        mLogic.SetSearch(mSearchBuf);
        DIA_LOG_DEBUG("Application", "BootMenuModule: search '%s'", mSearchBuf);
        OnRebuilt("search");
    }
    if (mLogic.HasActiveFilters())
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear filters"))
        {
            mLogic.ClearFilters();
            mSearchBuf[0] = '\0';
            mLogic.SetSearch("");
            DIA_LOG_DEBUG("Application", "BootMenuModule: filters cleared");
            OnRebuilt("filter");
        }
    }
}

void BootMenuModule::DrawGroup(unsigned int groupIndex)
{
    BootMenuLogic::GroupInfo info;
    if (!mLogic.GetGroup(groupIndex, info))
        return;

    ImGui::SetNextItemOpen(!info.collapsed, ImGuiCond_Always);
    char header[BootMenuLogic::kMaxTitle + 16];
    snprintf(header, sizeof(header), "%s###grp%u", info.title, static_cast<unsigned int>(info.group));
    const bool open = ImGui::CollapsingHeader(header);
    if (open == info.collapsed)
        mLogic.ToggleCollapsed(info.group);
    if (!open)
        return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    const unsigned int rowCount = mLogic.RowCount(groupIndex);
    for (unsigned int r = 0; r < rowCount; ++r)
    {
        BootMenuLogic::Row row;
        if (!mLogic.GetRow(groupIndex, r, row))
            continue;

        ImGui::PushID(static_cast<int>(groupIndex * 1000u + r));

        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float indent = row.isPreset ? kPresetIndent : 0.0f;
        const bool hasDesc = row.description[0] != '\0';
        const float height = (hasDesc ? 2.0f : 1.0f) * lineH;

        const bool selected = !row.isPreset && mLogic.IsSelected(row.stage);
        const bool presetSelected = row.isPreset && mLogic.IsSelected(row.stage) && mSelectedPreset == row.preset;
        const bool clicked = ImGui::Selectable("##row", selected || presetSelected,
                                               ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, height));
        const bool hovered = ImGui::IsItemHovered();

        if (clicked)
        {
            mLogic.Select(row.stage);
            mSelectedPreset = row.isPreset ? row.preset : Dia::Core::StringCRC();
            mSelectedGroup = info.group;
            if (ImGui::IsMouseDoubleClicked(0))
                Launch(row.stage, mSelectedPreset, info.group);
        }

        const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 dimCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        float x = start.x + indent + 4.0f;

        if (!row.isPreset)
        {
            if (mLogic.IsVisited(row.stage))
                dl->AddText(ImVec2(x, start.y), ImGui::GetColorU32(ImVec4(0.29f, 0.88f, 0.29f, 1.0f)), "\xe2\x97\x8f");
            else
                dl->AddText(ImVec2(x, start.y), dimCol, "\xe2\x97\x8b");
        }
        else
        {
            dl->AddText(ImVec2(x, start.y), dimCol, "\xe2\x86\xb3");
        }
        x += kNameIndent - 4.0f;
        dl->AddText(ImVec2(x, start.y), textCol, row.name);

        float rightPad = 8.0f;
        if (!row.isPreset)
        {
            BootMenuLogic::Badge badge;
            if (mLogic.GetBadge(row.stage, badge) && badge.label != nullptr && badge.label[0] != '\0')
            {
                const float bw = ImGui::CalcTextSize(badge.label).x;
                dl->AddText(ImVec2(start.x + width - bw - rightPad, start.y),
                            ImGui::GetColorU32(BadgeColourVec(badge.colour)), badge.label);
                rightPad += bw + 12.0f;
            }
        }

        if (hasDesc)
        {
            char fitted[256];
            const float avail = width - (x - start.x) - rightPad;
            FitText(row.description, avail, fitted, sizeof(fitted));
            dl->AddText(ImVec2(x, start.y + lineH), dimCol, fitted);
        }

        if (hovered && hasDesc)
            ImGui::SetTooltip("%s", row.description);

        ImGui::PopID();
    }
}

void BootMenuModule::DrawMenu()
{
    DIA_TRACE_ZONE("BootMenu.DrawMenu", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (mFirstDraw)
    {
        DIA_LOG_INFO("Application", "BootMenuModule: first draw (%u stages)", mLogic.StageCount());
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

    ImGui::TextDisabled("%u stages available", mLogic.StageCount());
    ImGui::Separator();

    DrawFilterBar();

    // Groups
    const float footerH = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f;
    if (ImGui::BeginChild("##groups", ImVec2(0.0f, -footerH), false))
    {
        if (mLogic.GroupCount() == 0)
            ImGui::TextDisabled(mLogic.StageCount() == 0 ? "No stages available" : "No stages match the current filters");
        for (unsigned int g = 0; g < mLogic.GroupCount(); ++g)
            DrawGroup(g);
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Launch
    const bool canLaunch = mLogic.HasSelection();
    if (!canLaunch)
        ImGui::BeginDisabled();

    if (ImGui::Button("Launch") ||
        (canLaunch && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Enter)))
    {
        Launch(mLogic.Selected(), mSelectedPreset, mSelectedGroup);
    }

    if (!canLaunch)
        ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextDisabled("Enter = Launch");

    ImGui::End();
}

void BootMenuModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mNavRequest.Connect(app);
    mSnapshotIn.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using BootMenuModule_ = Cluiche::AppFlow::BootMenuModule; }
DIA_MODULE(BootMenuModule_);
DIA_DESCRIBE(BootMenuModule_::kTypeId, "Boot menu over the stage catalogue: grouped stages, tag chips, search, launch presets and test result badges; falls back to a flat Boot transitions list when no catalogue arrives.");
