#ifdef DIA_DEBUG

#include "SelectionInspectorDrawer.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>

namespace Dia::EntityVisualDebugger
{

SelectionInspectorDrawer::SelectionInspectorDrawer(
    Dia::Entity::IEntityInspectable& inspectable,
    Dia::Entity::Domain& domain,
    const Dia::Core::IDebugContext& manager)
    : mInspectable(inspectable)
    , mDomain(domain)
    , mManager(manager)
{
}

Dia::Core::StringCRC SelectionInspectorDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityInspector;
}

void SelectionInspectorDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
