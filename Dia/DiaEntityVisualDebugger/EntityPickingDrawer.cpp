#ifdef DIA_DEBUG

#include "EntityPickingDrawer.h"
#include "EntityPositionHelper.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

static const Dia::Core::RGBA kSelectionColour(255, 255, 0, 255);

EntityPickingDrawer::EntityPickingDrawer(
    Dia::Entity::IEntityInspectable& inspectable,
    Dia::Entity::Domain& domain,
    const Dia::Core::IDebugContext& manager,
    Dia::Core::StringCRC positionComponentTypeId)
    : mInspectable(inspectable)
    , mDomain(domain)
    , mManager(manager)
    , mPositionTypeId(positionComponentTypeId)
{
}

Dia::Core::StringCRC EntityPickingDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityPicking;
}

void EntityPickingDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    uint32_t selectedId = mManager.GetSelectedEntityId();
    if (selectedId == 0)
        return;

    Dia::Entity::Entity entity = mDomain.GetAliveEntity(selectedId - 1);
    if (!entity.IsValid())
        return;

    const float scale = mManager.GetDebugScale();
    Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(mInspectable, entity, mPositionTypeId);
    draw.RequestDraw(pos, mHighlightRadius * scale, kSelectionColour);
}

void EntityPickingDrawer::DrawImGui()
{
    uint32_t selectedId = mManager.GetSelectedEntityId();
    if (selectedId == 0)
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::TextDisabled("Click a pickable entity to select");
    }
    else
    {
        Dia::Entity::Entity entity = mDomain.GetAliveEntity(selectedId - 1);
        if (entity.IsValid())
        {
            const char* name = mDomain.GetDebugName(entity);
            ImGui::Text("Selected: %s (id=%u)", name ? name : "(unnamed)", selectedId);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Selected entity no longer alive");
        }

        if (ImGui::Button("Clear selection"))
        {
            const_cast<Dia::Core::IDebugContext&>(mManager).SetSelectedEntityId(0);
        }
    }

    ImGui::SliderFloat("Highlight radius", &mHighlightRadius, 4.0f, 32.0f);
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
