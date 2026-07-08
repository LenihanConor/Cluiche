#ifdef DIA_DEBUG

#include "HierarchyLinesDrawer.h"
#include "EntityPositionHelper.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
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

void HierarchyLinesDrawer::Draw(Dia::Core::IDebugDraw& draw)
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
            draw.RequestDraw(parentPos, childPos, Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

void HierarchyLinesDrawer::DrawImGui()
{
    ImGui::Checkbox("Show orphans", &mShowOrphans);
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
