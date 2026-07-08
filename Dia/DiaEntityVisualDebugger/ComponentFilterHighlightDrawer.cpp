#ifdef DIA_DEBUG

#include "ComponentFilterHighlightDrawer.h"
#include "EntityPositionHelper.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

ComponentFilterHighlightDrawer::ComponentFilterHighlightDrawer(
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

Dia::Core::StringCRC ComponentFilterHighlightDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityHighlight;
}

void ComponentFilterHighlightDrawer::SetFilterTypeId(Dia::Core::StringCRC typeId)
{
    mFilterTypeId = typeId;
}

void ComponentFilterHighlightDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (mFilterTypeId == Dia::Core::StringCRC())
        return;

    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain> entities;
    mInspectable.GetAllEntities(entities);

    const float scale = mManager.GetDebugScale();
    const float radius = mHighlightRadius * scale;

    for (unsigned int i = 0; i < entities.Size(); ++i)
    {
        Dia::Entity::Entity entity = entities[i];
        if (!mDomain.HasComponentByTypeId(entity, mFilterTypeId))
            continue;

        Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(mInspectable, entity, mPositionTypeId);
        draw.RequestDraw(pos, radius, Dia::Debug::DebugColourPalette::kPinned);
    }
}

void ComponentFilterHighlightDrawer::DrawImGui()
{
    auto& registry = Dia::Entity::ComponentRegistry::Get();
    const uint32_t count = registry.GetCount();

    const char* preview = "(none)";
    if (mSelectedIndex >= 0 && static_cast<uint32_t>(mSelectedIndex) < count)
        preview = registry.GetByIndex(static_cast<uint32_t>(mSelectedIndex)).debugName;

    if (ImGui::BeginCombo("Component type", preview))
    {
        if (ImGui::Selectable("(none)", mSelectedIndex < 0))
        {
            mSelectedIndex = -1;
            mFilterTypeId = Dia::Core::StringCRC();
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto& desc = registry.GetByIndex(i);
            bool isSelected = (static_cast<uint32_t>(mSelectedIndex) == i);
            if (ImGui::Selectable(desc.debugName, isSelected))
            {
                mSelectedIndex = static_cast<int>(i);
                mFilterTypeId = desc.typeId;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SliderFloat("Highlight radius", &mHighlightRadius, 2.0f, 32.0f);
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
