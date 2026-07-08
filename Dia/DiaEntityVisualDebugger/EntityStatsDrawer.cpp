#ifdef DIA_DEBUG

#include "EntityStatsDrawer.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

EntityStatsDrawer::EntityStatsDrawer(Dia::Entity::IEntityInspectable& inspectable)
    : mInspectable(inspectable)
{
}

Dia::Core::StringCRC EntityStatsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityStats;
}

void EntityStatsDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
}

void EntityStatsDrawer::DrawImGui()
{
    ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240.f, 120.f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Entity Stats");

    const uint32_t entityCount = mInspectable.GetEntityCount();
    const uint32_t componentTypeCount = Dia::Entity::ComponentRegistry::Get().GetCount();

    ImGui::Text("Entities: %u / %u", entityCount, Dia::Entity::kMaxEntitiesPerDomain);

    float utilisation = static_cast<float>(entityCount) / static_cast<float>(Dia::Entity::kMaxEntitiesPerDomain);
    ImGui::ProgressBar(utilisation, ImVec2(-1.f, 0.f), "");
    ImGui::Text("Component types: %u / %u", componentTypeCount, Dia::Entity::kMaxComponentTypesPerDomain);

    ImGui::End();
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
