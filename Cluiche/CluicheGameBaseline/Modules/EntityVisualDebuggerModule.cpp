#include "Modules/EntityVisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaEntityVisualDebugger/EntityLabelsDrawer.h>
#include <DiaEntityVisualDebugger/EntityStatsDrawer.h>
#include <DiaEntityVisualDebugger/HierarchyLinesDrawer.h>
#include <DiaEntityVisualDebugger/ComponentFilterHighlightDrawer.h>
#include <DiaEntityVisualDebugger/EntityPickingDrawer.h>
#include <DiaEntityVisualDebugger/SelectionInspectorDrawer.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaPicking/PickEvent.h>
#include <DiaPicking/PickTrigger.h>
#include <diaentitytemplate/ComponentRegistry.h>
#include <diaentitytemplate/ComponentTypeDesc.h>
#include "Modules/EntityModule.h"
#include "Modules/VisualDebuggerModule.h"
#include "Modules/PickingModule.h"

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC EntityVisualDebuggerModule::kTypeId("EntityVisualDebuggerModule");

EntityVisualDebuggerModule::EntityVisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

EntityVisualDebuggerModule::~EntityVisualDebuggerModule() = default;

Dia::ApplicationFlow::StartResult EntityVisualDebuggerModule::DoStart()
{
    if (mPositionTypeId == Dia::Core::StringCRC())
    {
        auto& registry = Dia::Entity::ComponentRegistry::Get();
        for (uint32_t i = 0; i < registry.GetCount(); ++i)
        {
            const auto& desc = registry.GetByIndex(i);
            bool hasX = false, hasY = false;
            for (uint16_t f = 0; f < desc.fieldCount; ++f)
            {
                if (strcmp(desc.fields[f].name, "x") == 0) hasX = true;
                if (strcmp(desc.fields[f].name, "y") == 0) hasY = true;
            }
            if (hasX && hasY)
            {
                mPositionTypeId = desc.typeId;
                break;
            }
        }
    }

    RegisterDrawers();

    if (auto* picking = mPickingRef.Get())
    {
        mPickSubscriberId.value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
        picking->GetRouter().SubscribeToTrigger(Dia::Picking::PickTrigger::kClick, mPickSubscriberId);
        mPickingSubscribed = true;
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void EntityVisualDebuggerModule::DoUpdate(float)
{
    if (!mPickingSubscribed) return;

    auto* picking = mPickingRef.Get();
    if (!picking) return;

    auto* vd = mVisualDebuggerRef.Get();
    if (!vd) return;

    using PickEvent2D = Dia::Picking::PickEvent<Dia::Geometry2DPicking::PickHit2D>;
    picking->GetMailbox().Drain<PickEvent2D>(
        [&vd](const Dia::Mailbox::Address&, const PickEvent2D& evt)
        {
            if (evt.trigger != Dia::Picking::PickTrigger::kClick) return;

            if (!evt.hits.HasHit())
            {
                vd->GetLayerManager().SetSelectedEntityId(0);
                return;
            }

            const auto& best = evt.hits.Best();
            if (best.kind == Dia::Geometry2DPicking::PickHit2D::Kind::kEntity)
            {
                vd->GetLayerManager().SetSelectedEntityId(best.entityId + 1);
            }
            else if (best.kind == Dia::Geometry2DPicking::PickHit2D::Kind::kObject)
            {
                vd->GetLayerManager().SetSelectedEntityId(best.objectIdx + 1);
            }
        });
}

Dia::ApplicationFlow::StopResult EntityVisualDebuggerModule::DoStop()
{
    if (mPickingSubscribed)
    {
        if (auto* picking = mPickingRef.Get())
        {
            picking->GetRouter().UnsubscribeFromTrigger(Dia::Picking::PickTrigger::kClick, mPickSubscriberId);
        }
        mPickingSubscribed = false;
    }

    UnregisterDrawers();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void EntityVisualDebuggerModule::RegisterDrawers()
{
    auto* entityMod = mEntityRef.Get();
    auto* vdMod = mVisualDebuggerRef.Get();
    if (!entityMod || !vdMod) return;

    auto& domain = entityMod->GetDomain();
    auto& inspectable = entityMod->GetInspectable();
    auto& layerManager = vdMod->GetLayerManager();

    mLabelsDrawer    = std::make_unique<Dia::EntityVisualDebugger::EntityLabelsDrawer>(inspectable, domain, layerManager, mPositionTypeId);
    mStatsDrawer     = std::make_unique<Dia::EntityVisualDebugger::EntityStatsDrawer>(inspectable);
    mHierarchyDrawer = std::make_unique<Dia::EntityVisualDebugger::HierarchyLinesDrawer>(inspectable, domain, layerManager, mPositionTypeId);
    mHighlightDrawer = std::make_unique<Dia::EntityVisualDebugger::ComponentFilterHighlightDrawer>(inspectable, domain, layerManager, mPositionTypeId);
    mPickingDrawer   = std::make_unique<Dia::EntityVisualDebugger::EntityPickingDrawer>(inspectable, domain, layerManager, mPositionTypeId);
    mInspectorDrawer = std::make_unique<Dia::EntityVisualDebugger::SelectionInspectorDrawer>(inspectable, domain, layerManager);

    layerManager.Register(mLabelsDrawer.get(),    55, Dia::Debug::LayerNames::kEntityStageTag);
    layerManager.Register(mHierarchyDrawer.get(), 56, Dia::Debug::LayerNames::kEntityStageTag);
    layerManager.Register(mHighlightDrawer.get(), 57, Dia::Debug::LayerNames::kEntityStageTag);
    layerManager.Register(mPickingDrawer.get(),   58, Dia::Debug::LayerNames::kEntityStageTag);
    layerManager.Register(mStatsDrawer.get(),     59, Dia::Debug::LayerNames::kEntityStageTag);
    layerManager.Register(mInspectorDrawer.get(), 60, Dia::Debug::LayerNames::kEntityStageTag);
}

void EntityVisualDebuggerModule::UnregisterDrawers()
{
    if (auto* vdMod = mVisualDebuggerRef.Get())
    {
        auto& lm = vdMod->GetLayerManager();
        lm.Unregister(Dia::Debug::LayerNames::kEntityLabels);
        lm.Unregister(Dia::Debug::LayerNames::kEntityHierarchy);
        lm.Unregister(Dia::Debug::LayerNames::kEntityHighlight);
        lm.Unregister(Dia::Debug::LayerNames::kEntityPicking);
        lm.Unregister(Dia::Debug::LayerNames::kEntityStats);
        lm.Unregister(Dia::Debug::LayerNames::kEntityInspector);
    }

    mLabelsDrawer.reset();
    mStatsDrawer.reset();
    mHierarchyDrawer.reset();
    mHighlightDrawer.reset();
    mPickingDrawer.reset();
    mInspectorDrawer.reset();
}

} } // namespace Cluiche::AppFlow

namespace { using EntityVisualDebuggerModule_ = Cluiche::AppFlow::EntityVisualDebuggerModule; }
DIA_MODULE(EntityVisualDebuggerModule_);
DIA_DESCRIBE(EntityVisualDebuggerModule_::kTypeId, "Entity debug overlays: labels, hierarchy, stats, picking, inspector");

#endif // DIA_DEBUG
