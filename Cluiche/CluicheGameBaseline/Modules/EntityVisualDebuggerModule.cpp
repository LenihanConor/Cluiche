#include "Modules/EntityVisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaPicking/PickEvent.h>
#include <DiaPicking/PickTrigger.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
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

    RegisterDebugDomain();

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

    UnregisterDebugDomain();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void EntityVisualDebuggerModule::RegisterDebugDomain()
{
    auto* entityMod = mEntityRef.Get();
    auto* vdMod = mVisualDebuggerRef.Get();
    if (!entityMod || !vdMod) return;

    mDebugDomain = std::make_unique<Dia::EntityVisualDebugger::EntityDebugDomain>(
        entityMod->GetInspectable(), entityMod->GetDomain(), mPositionTypeId);

    vdMod->RegisterDomain(*mDebugDomain);
}

void EntityVisualDebuggerModule::UnregisterDebugDomain()
{
    if (!mDebugDomain) return;

    // Only unregister if VisualDebuggerModule is still active (concurrent stop).
    if (auto* vdMod = mVisualDebuggerRef.Get())
        vdMod->UnregisterDomain(*mDebugDomain);

    mDebugDomain.reset();
}

} } // namespace Cluiche::AppFlow

namespace { using EntityVisualDebuggerModule_ = Cluiche::AppFlow::EntityVisualDebuggerModule; }
DIA_MODULE(EntityVisualDebuggerModule_);
DIA_DESCRIBE(EntityVisualDebuggerModule_::kTypeId, "Entity debug overlays: labels, hierarchy, stats, picking, inspector");

#endif // DIA_DEBUG
