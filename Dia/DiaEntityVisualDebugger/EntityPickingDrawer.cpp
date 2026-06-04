#ifdef DIA_DEBUG

#include "EntityPickingDrawer.h"
#include "EntityPositionHelper.h"
#include <diaentitytemplate/IEntityInspectable.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/Entity.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

static const Dia::Graphics::RGBA kSelectionColour(255, 255, 0, 255);

EntityPickingDrawer::EntityPickingDrawer(
    Dia::Entity::IEntityInspectable& inspectable,
    Dia::Entity::Domain& domain,
    const Dia::Debug::DebugLayerManager& manager,
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

void EntityPickingDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    uint32_t selectedId = mManager.GetSelectedEntityId();
    if (selectedId == 0)
        return;

    Dia::Entity::Entity entity = mDomain.GetAliveEntity(selectedId - 1);
    if (!entity.IsValid())
        return;

    const float scale = mManager.GetDebugScale();
    Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(mInspectable, entity, mPositionTypeId);
    frameData.RequestDraw(pos, mHighlightRadius * scale, kSelectionColour);
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
            const_cast<Dia::Debug::DebugLayerManager&>(mManager).SetSelectedEntityId(0);
        }
    }

    ImGui::SliderFloat("Highlight radius", &mHighlightRadius, 4.0f, 32.0f);
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
