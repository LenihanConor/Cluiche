#ifdef DIA_DEBUG

#include "HierarchyLinesDrawer.h"
#include "EntityPositionHelper.h"
#include <diaentitytemplate/IEntityInspectable.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/Entity.h>
#include <diaentitytemplate/Hierarchy/ChildBufferComponent.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

HierarchyLinesDrawer::HierarchyLinesDrawer(
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

Dia::Core::StringCRC HierarchyLinesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityHierarchy;
}

void HierarchyLinesDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain> entities;
    mInspectable.GetAllEntities(entities);

    for (unsigned int i = 0; i < entities.Size(); ++i)
    {
        Dia::Entity::Entity parent = entities[i];
        auto* childBuffer = mDomain.GetComponent<Dia::Entity::Hierarchy::ChildBufferComponent>(parent);
        if (!childBuffer) continue;

        Dia::Maths::Vector2D parentPos = EntityPositionHelper::GetPosition(mInspectable, parent, mPositionTypeId);

        const auto& children = childBuffer->children;
        for (unsigned int c = 0; c < children.Size(); ++c)
        {
            Dia::Entity::Entity child = children[c];
            if (!mDomain.IsAlive(child)) continue;

            Dia::Maths::Vector2D childPos = EntityPositionHelper::GetPosition(mInspectable, child, mPositionTypeId);
            frameData.RequestDraw(parentPos, childPos, Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

void HierarchyLinesDrawer::DrawImGui()
{
    ImGui::Checkbox("Show orphans", &mShowOrphans);
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
