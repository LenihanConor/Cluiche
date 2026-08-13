#ifdef DIA_DEBUG

#include "EntityStatsDrawer.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

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

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
