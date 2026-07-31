////////////////////////////////////////////////////////////////////////////////
// Filename: UtilityScoreDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaUtilityAIVisualDebugger/UtilityScoreDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <imgui.h>

namespace Dia::UtilityAI
{

UtilityScoreDrawer::UtilityScoreDrawer(const UtilitySet& utilitySet)
    : mUtilitySet(utilitySet)
{}

Dia::Core::StringCRC UtilityScoreDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kUtilityAIScores;
}

void UtilityScoreDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
    // No world-space representation — all output in DrawImGui().
}

void UtilityScoreDrawer::DrawImGui()
{
    DIA_TRACE_ZONE("utility_ai.scores", ::Dia::Observation::Trace::Category::kDiaApplicationFlow);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> ids;
    Dia::Core::Containers::DynamicArrayC<float, 32>                scores;
    mUtilitySet.GetLastFrameScores(ids, scores);

    if (ids.Size() == 0)
    {
        ImGui::TextDisabled("No scores (Evaluate() not called yet)");
        return;
    }

    if (ImGui::BeginTable("utility_scores", 3,
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("Score",   ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Bar",     ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableHeadersRow();

        for (unsigned int i = 0; i < ids.Size(); ++i)
        {
            const float score = scores[i];

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(ids[i].AsChar());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", score);

            ImGui::TableSetColumnIndex(2);
            char barLabel[16];
            std::snprintf(barLabel, sizeof(barLabel), "##bar%u", i);
            ImGui::ProgressBar(score, ImVec2(-1.0f, 0.0f), barLabel);
        }

        ImGui::EndTable();
    }
}

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
