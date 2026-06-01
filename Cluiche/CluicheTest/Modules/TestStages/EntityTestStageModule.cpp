#include "Modules/TestStages/EntityTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaEntity/Hierarchy/Hierarchy.h>
#include <DiaEntity/EntityAddress.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaCore/Json/external/json/json.h>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/EntityTestDrawer.h"
#include <DiaGeometry2DPicking/PickingService2D.h>
#include <DiaPicking/PickTrigger.h>
#endif

namespace CluicheTest {

const Dia::Core::StringCRC EntityTestStageModule::kTypeId("EntityTestStageModule");

EntityTestStageModule::EntityTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

EntityTestStageModule::~EntityTestStageModule() = default;

Dia::Core::StringCRC EntityTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("EntityTestStage");
}

const Dia::Core::StringCRC* EntityTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("entity.spawn_complete"),
        Dia::Core::StringCRC("entity.query_correct"),
        Dia::Core::StringCRC("entity.hierarchy_valid"),
        Dia::Core::StringCRC("entity.destroy_cascade"),
        Dia::Core::StringCRC("entity.mailbox_received"),
        Dia::Core::StringCRC("entity.lifecycle_complete"),
    };
    outCount = 6;
    return names;
}

bool EntityTestStageModule::AreDependenciesReady()
{
    auto* em = mEntityModule.Get();
    return em && em->IsReady();
}

void EntityTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupScene();
    RegisterCheckpoints(service);
}

void EntityTestStageModule::SetupScene()
{
    auto* em = mEntityModule.Get();
    auto& domain = em->GetDomain();

    // Inject lifecycle counters into component statics before pool registration
    TransformComponent::sAttachCounter         = &mOnAttachCount;
    TransformComponent::sDetachCounter         = &mOnDetachCount;
    TransformComponent::sMailboxReceiveCounter = &mMailboxReceiveCount;
    VisualTestRenderComponent::sAttachCounter  = &mOnAttachCount;
    VisualTestRenderComponent::sDetachCounter  = &mOnDetachCount;

    // Register component pools (hierarchy pools already registered by EntityModule)
    domain.RegisterPool(new Dia::Entity::ComponentPool<TransformComponent>(
        TransformComponent::kTypeId));
    domain.RegisterPool(new Dia::Entity::ComponentPool<VisualTestRenderComponent>(
        VisualTestRenderComponent::kTypeId));
    domain.RegisterPool(new Dia::Entity::ComponentPool<PickableCircleComponent>(
        PickableCircleComponent::kTypeId));

    // Register mailbox type for ping messages
    domain.GetMailbox().RegisterType<Dia::Core::StringCRC, 32>();

    // --- Hierarchy group (4 entities, blue #4FC3F7) ---
    unsigned int entityIdx = 0;

    mParentEntity = domain.CreateEntity("Parent");
    {
        Json::Value cfg; cfg["x"] = 300.f; cfg["y"] = 100.f;
        domain.QueueAddComponentByTypeId(mParentEntity, TransformComponent::kTypeId, cfg);
    }
    {
        Json::Value cfg; cfg["radius"] = 18.f; cfg["colour"] = 0x4FC3F7FFu;
        domain.QueueAddComponentByTypeId(mParentEntity, VisualTestRenderComponent::kTypeId, cfg);
    }
    {
        Json::Value cfg; cfg["objectIdx"] = entityIdx++;
        domain.QueueAddComponentByTypeId(mParentEntity, PickableCircleComponent::kTypeId, cfg);
    }

    const char* childNames[] = { "ChildA", "ChildB", "ChildC" };
    Dia::Entity::Entity* childHandles[] = { &mChildA, &mChildB, &mChildC };
    const float childOffsets[][2] = { {-80.f, 80.f}, {0.f, 80.f}, {80.f, 80.f} };

    for (int i = 0; i < 3; ++i)
    {
        *childHandles[i] = domain.CreateEntity(childNames[i]);
        {
            Json::Value cfg;
            cfg["x"] = 300.f + childOffsets[i][0];
            cfg["y"] = 100.f + childOffsets[i][1];
            domain.QueueAddComponentByTypeId(*childHandles[i], TransformComponent::kTypeId, cfg);
        }
        {
            Json::Value cfg; cfg["radius"] = 14.f; cfg["colour"] = 0x4FC3F7FFu;
            domain.QueueAddComponentByTypeId(*childHandles[i], VisualTestRenderComponent::kTypeId, cfg);
        }
        {
            Json::Value cfg; cfg["objectIdx"] = entityIdx++;
            domain.QueueAddComponentByTypeId(*childHandles[i], PickableCircleComponent::kTypeId, cfg);
        }
        Dia::Entity::Hierarchy::QueueSetParent(domain, *childHandles[i], mParentEntity);
    }

    // --- Query targets (4 entities, green #66BB6A) ---
    const char* queryNames[] = { "Query0", "Query1", "Query2", "Query3" };
    const float queryPos[][2] = { {-100.f, 200.f}, {0.f, 200.f}, {-100.f, 300.f}, {0.f, 300.f} };
    for (int i = 0; i < 4; ++i)
    {
        mQueryEntities[i] = domain.CreateEntity(queryNames[i]);
        {
            Json::Value cfg; cfg["x"] = queryPos[i][0]; cfg["y"] = queryPos[i][1];
            domain.QueueAddComponentByTypeId(mQueryEntities[i], TransformComponent::kTypeId, cfg);
        }
        {
            Json::Value cfg; cfg["radius"] = 14.f; cfg["colour"] = 0x66BB6AFFu;
            domain.QueueAddComponentByTypeId(mQueryEntities[i], VisualTestRenderComponent::kTypeId, cfg);
        }
        {
            Json::Value cfg; cfg["objectIdx"] = entityIdx++;
            domain.QueueAddComponentByTypeId(mQueryEntities[i], PickableCircleComponent::kTypeId, cfg);
        }
    }

    // --- Doomed entity (1, grey) ---
    mDoomedEntity = domain.CreateEntity("Doomed");
    {
        Json::Value cfg; cfg["x"] = 200.f; cfg["y"] = 200.f;
        domain.QueueAddComponentByTypeId(mDoomedEntity, TransformComponent::kTypeId, cfg);
    }
    {
        Json::Value cfg; cfg["radius"] = 14.f; cfg["colour"] = 0x888888FFu;
        domain.QueueAddComponentByTypeId(mDoomedEntity, VisualTestRenderComponent::kTypeId, cfg);
    }
    {
        Json::Value cfg; cfg["objectIdx"] = entityIdx++;
        domain.QueueAddComponentByTypeId(mDoomedEntity, PickableCircleComponent::kTypeId, cfg);
    }

    // Apply all queued ops — OnAttach fires here, so inject services first
#ifdef DIA_DEBUG
    if (auto* picking = mPickingRef.Get())
        PickableCircleComponent::sPickingService = &picking->GetService();
#endif
    domain.EndOfFrame();

    // Build flat entity list for indexed picking
    mEntityCount = 0;
    mAllEntities[mEntityCount++] = mParentEntity;
    mAllEntities[mEntityCount++] = mChildA;
    mAllEntities[mEntityCount++] = mChildB;
    mAllEntities[mEntityCount++] = mChildC;
    for (int i = 0; i < 4; ++i)
        mAllEntities[mEntityCount++] = mQueryEntities[i];
    mAllEntities[mEntityCount++] = mDoomedEntity;  // index 8 — gone after frame 0

    mSceneBuilt = true;
}

void EntityTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entity.spawn_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mSpawnComplete, mSpawnComplete ? "9 entities alive after setup" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entity.query_correct"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mQueryCorrect, mQueryCorrect ? "query returns 8 after destroy" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entity.hierarchy_valid"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mHierarchyValid, mHierarchyValid ? "parent has 3 children, each links back" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entity.destroy_cascade"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mDestroyCascade, mDestroyCascade ? "doomed entity !IsAlive after EndOfFrame" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entity.mailbox_received"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mMailboxReceived, mMailboxReceived ? "mailbox broadcast received" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entity.lifecycle_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mLifecycleComplete, mLifecycleComplete ? "18 attaches, 2 detaches" : "pending", 0.f };
        });
}

bool EntityTestStageModule::AllCheckpointsPassed() const
{
    return mSpawnComplete && mQueryCorrect && mHierarchyValid
        && mDestroyCascade && mMailboxReceived && mLifecycleComplete;
}

void EntityTestStageModule::OnUpdate(float /*deltaTime*/)
{
    auto* em = mEntityModule.Get();
    if (!em) return;

    auto& domain = em->GetDomain();

    // Frame 0: queue destroy on doomed entity (EntityModule applies it at EndOfFrame)
    if (GetFrameCount() == 1 && !mDoomedDestroyed)
    {
        domain.QueueDestroy(mDoomedEntity);
        mDoomedDestroyed = true;
    }

    // Every 10 frames: broadcast ping so TransformComponent::DoUpdate counts it
    if (GetFrameCount() % 10 == 0)
    {
        domain.GetMailbox().Send(
            Dia::Entity::MakeAllAddress(),
            Dia::Core::StringCRC("entity.ping"));
    }

    // Evaluate checkpoint conditions
    if (!mSpawnComplete)
        mSpawnComplete = (domain.GetEntityCount() >= 9);

    if (mDoomedDestroyed && !mDestroyCascade)
        mDestroyCascade = !domain.IsAlive(mDoomedEntity);

    if (mDestroyCascade && !mQueryCorrect)
    {
        auto view = domain.Query<TransformComponent, VisualTestRenderComponent>();
        mQueryCorrect = (view.Count() == 8);
    }

    if (!mHierarchyValid)
    {
        auto* cb = domain.GetComponent<Dia::Entity::Hierarchy::ChildBufferComponent>(mParentEntity);
        if (cb && cb->children.Size() == 3)
        {
            bool ok = true;
            const Dia::Entity::Entity children[] = { mChildA, mChildB, mChildC };
            for (int i = 0; i < 3; ++i)
            {
                auto* p = domain.GetComponent<Dia::Entity::Hierarchy::ParentComponent>(children[i]);
                if (!p || p->GetParentEntity() != mParentEntity)
                { ok = false; break; }
            }
            mHierarchyValid = ok;
        }
    }

    if (mMailboxReceiveCount > 0)
        mMailboxReceived = true;

    // 9 entities × 2 components = 18 attaches; doomed × 2 = 2 detaches
    if (mOnAttachCount == 18 && mOnDetachCount == 2)
        mLifecycleComplete = true;

    // Emit metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    static Dia::Observation::Metric::Gauge* sSpawnCount = reg.RegisterGauge(
        Dia::Core::StringCRC("cluichetest.entity.spawn_count"));
    static Dia::Observation::Metric::Gauge* sAliveCount = reg.RegisterGauge(
        Dia::Core::StringCRC("cluichetest.entity.alive_count"));
    static Dia::Observation::Metric::Gauge* sMailboxCount = reg.RegisterGauge(
        Dia::Core::StringCRC("cluichetest.entity.mailbox_count"));

    if (sSpawnCount) sSpawnCount->Set(static_cast<double>(mOnAttachCount / 2));
    if (sAliveCount) sAliveCount->Set(static_cast<double>(domain.GetEntityCount()));
    if (sMailboxCount) sMailboxCount->Set(static_cast<double>(mMailboxReceiveCount));

    // Report pass
    if (AllCheckpointsPassed() && !IsResolved())
        ReportPassed();

#ifdef DIA_DEBUG
    // Lazy-init drawer (wait for VisualDebuggerModule) + subscribe to pick events
    if (!mDrawer)
    {
        auto* vd      = mVisualDebuggerRef.Get();
        auto* picking = mPickingRef.Get();
        if (vd && picking)
        {
            mDrawer = std::make_unique<EntityTestDrawer>(
                domain, mParentEntity, mChildA, mChildB, mChildC,
                mQueryEntities, mDoomedEntity, mDoomedDestroyed,
                mOnAttachCount, mOnDetachCount, mMailboxReceiveCount,
                mHasSelection, mSelectedIdx, mAllEntities, mEntityCount,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 20, Dia::Core::StringCRC("Entity"));

            mPickSubscriberId.value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
            picking->GetRouter().SubscribeToTrigger(Dia::Picking::PickTrigger::kClick, mPickSubscriberId);
            mPickingRegistered = true;
        }
    }

    // Drain pick events each frame
    if (mPickingRegistered)
    {
        if (auto* picking = mPickingRef.Get())
        {
            using PickEvent2D = Dia::Picking::PickEvent<Dia::Geometry2DPicking::PickHit2D>;
            picking->GetMailbox().Drain<PickEvent2D>(
                [this](const Dia::Mailbox::Address& /*addr*/, const PickEvent2D& evt)
                {
                    if (evt.trigger != Dia::Picking::PickTrigger::kClick) return;
                    if (!evt.hits.HasHit())
                    {
                        mHasSelection = false;
                        return;
                    }
                    const auto& best = evt.hits.Best();
                    if (best.kind == Dia::Geometry2DPicking::PickHit2D::Kind::kObject)
                    {
                        mHasSelection = true;
                        mSelectedIdx  = best.objectIdx;
                    }
                });
        }
    }
#endif
}

void EntityTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    // PickableCircleComponent::OnDetach handles per-entity unregistration when the
    // domain is torn down. Clear the static service pointer so stale OnDetach calls
    // after module stop are safe no-ops.
    PickableCircleComponent::sPickingService = nullptr;

    if (auto* picking = mPickingRef.Get())
    {
        if (mPickingRegistered)
        {
            picking->GetRouter().UnsubscribeFromTrigger(Dia::Picking::PickTrigger::kClick, mPickSubscriberId);
            mPickingRegistered = false;
        }
    }

    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }

    mHasSelection = false;
    mSelectedIdx  = 0;
#endif

    // Reset state for possible re-entry
    mSceneBuilt      = false;
    mDoomedDestroyed = false;
    mOnAttachCount   = 0;
    mOnDetachCount   = 0;
    mMailboxReceiveCount = 0;
    mSpawnComplete     = false;
    mQueryCorrect      = false;
    mHierarchyValid    = false;
    mDestroyCascade    = false;
    mMailboxReceived   = false;
    mLifecycleComplete = false;
}

} // namespace CluicheTest

namespace { using EntityTestStageModule_ = CluicheTest::EntityTestStageModule; }
DIA_MODULE(EntityTestStageModule_);
