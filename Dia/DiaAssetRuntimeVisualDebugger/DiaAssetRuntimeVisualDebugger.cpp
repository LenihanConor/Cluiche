#include "DiaAssetRuntimeVisualDebugger/DiaAssetRuntimeVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <imgui.h>

namespace Dia
{
    namespace AssetRuntime
    {

Dia::Core::StringCRC DiaAssetRuntimeVisualDebugger::GetLayerName() const
{
    return Dia::Debug::LayerNames::kAssetRuntime;
}

void DiaAssetRuntimeVisualDebugger::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
    DIA_TRACE_ZONE("asset.runtime", ::Dia::Observation::Trace::Category::kDiaGraphics);
}

void DiaAssetRuntimeVisualDebugger::DrawImGui()
{
    auto& metricReg = Dia::Observation::Metric::MetricRegistry::Instance();

    Dia::Observation::Metric::Gauge* gLoadCount = metricReg.FindGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_count"));
    Dia::Observation::Metric::Gauge* gHandles   = metricReg.FindGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.active_handles"));
    Dia::Observation::Metric::Gauge* gLoadTime  = metricReg.FindGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_time_ms"));

    const int loadCount  = gLoadCount ? static_cast<int>(gLoadCount->Value()) : 0;
    const int handles    = gHandles   ? static_cast<int>(gHandles->Value())   : 0;
    const float loadTime = gLoadTime  ? static_cast<float>(gLoadTime->Value()) : 0.0f;

    // --- Stage Load State ---
    ImGui::TextDisabled("Stage Load State");
    ImGui::Separator();

    ImGui::Text("Stage:"); ImGui::SameLine(120.0f);
    ImGui::Text("AssetRuntimeTestStage");

    ImGui::Text("Progress:"); ImGui::SameLine(120.0f);
    if (loadCount > 0)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%d / %d loaded", loadCount, loadCount);
    else
        ImGui::TextDisabled("idle");

    ImGui::Text("Failed:"); ImGui::SameLine(120.0f);
    ImGui::Text("0");

    ImGui::Spacing();

    // --- Assets ---
    ImGui::TextDisabled("Assets");
    ImGui::Separator();

    struct AssetEntry { const char* id; const char* icon; ImVec4 color; };
    const AssetEntry assets[] = {
        { "texture.ar_tex1", " T ",  ImVec4(0.3f, 0.6f, 0.9f, 1.0f) },
        { "texture.ar_tex2", " T ",  ImVec4(0.3f, 0.6f, 0.9f, 1.0f) },
        { "texture.ar_tex3", " T ",  ImVec4(0.3f, 0.6f, 0.9f, 1.0f) },
        { "json.ar_config",  " {} ", ImVec4(0.4f, 0.8f, 0.4f, 1.0f) },
    };

    for (const auto& entry : assets)
    {
        ImGui::TextColored(entry.color, "%s", entry.icon);
        ImGui::SameLine();
        ImGui::Text("%s", entry.id);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50.0f);
        if (loadCount > 0)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "LOADED");
        else
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.0f, 1.0f), "loading");
    }

    ImGui::Spacing();

    // --- Metrics ---
    ImGui::TextDisabled("Metrics");
    ImGui::Separator();

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 150.0f);

    ImGui::TextDisabled("loaded");       ImGui::NextColumn();
    ImGui::Text("%d", loadCount);        ImGui::NextColumn();

    ImGui::TextDisabled("active_handles"); ImGui::NextColumn();
    ImGui::Text("%d", handles);            ImGui::NextColumn();

    ImGui::TextDisabled("load_time_ms"); ImGui::NextColumn();
    ImGui::Text("%.1f", loadTime);       ImGui::NextColumn();

    ImGui::Columns(1);
}

    } // namespace AssetRuntime
} // namespace Dia

#endif // DIA_DEBUG
