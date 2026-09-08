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

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
