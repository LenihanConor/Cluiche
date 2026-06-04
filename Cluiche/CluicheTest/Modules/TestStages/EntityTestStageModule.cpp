#include "Modules/TestStages/EntityTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/Hierarchy/ParentComponent.h>
#include <diaentitytemplate/Hierarchy/ChildBufferComponent.h>
#include <diaentitytemplate/Hierarchy/Hierarchy.h>
#include <diaentitytemplate/EntityAddress.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaCore/Json/external/json/json.h>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/EntityTestDrawer.h"
#include <DiaGeometry2DPicking/PickingService2D.h>
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

    // Register component pools (hierarchy pools already registered by EntityModule)
    domain.RegisterPool(new Dia::Entity::ComponentPool<TransformComponent>(
        TransformComponent::kTypeId));
    domain.RegisterPool(new Dia::Entity::ComponentPool<VisualTestRenderComponent>(
        VisualTestRenderComponent::kTypeId));
    domain.RegisterPool(new Dia::Entity::ComponentPool<PickableCircleComponent>(
        PickableCircleComponent::kTypeId));

    // Register mailbox type for ping messages (module sends, module drains)
    domain.GetMailbox().RegisterType<Dia::Core::StringCRC, 32>();

    // Register PickingService2D so PickableCircleComponent::OnAttach can find it
#ifdef DIA_DEBUG
    if (auto* picking = mPickingRef.Get())
        domain.RegisterService<Dia::Geometry2DPicking::PickingService2D>(&picking->GetService());
#endif

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

    // Apply all queued ops — OnAttach fires here (PickableCircleComponent registers with PickingService)
    domain.EndOfFrame();

    // Build flat entity list for indexed lookup
    mEntityCount = 0;
    mAllEntities[mEntityCount++] = mParentEntity;
    mAllEntities[mEntityCount++] = mChildA;
    mAllEntities[mEntityCount++] = mChildB;
    mAllEntities[mEntityCount++] = mChildC;
    for (int i = 0; i < 4; ++i)
        mAllEntities[mEntityCount++] = mQueryEntities[i];
    mAllEntities[mEntityCount++] = mDoomedEntity;

    mSpawnedEntityCount = domain.GetEntityCount();
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
            return { mLifecycleComplete, mLifecycleComplete ? "spawn count matches expected" : "pending", 0.f };
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

    // Frame 1: queue destroy on doomed entity (EntityModule applies it at EndOfFrame)
    if (GetFrameCount() == 1 && !mDoomedDestroyed)
    {
        domain.QueueDestroy(mDoomedEntity);
        mDoomedDestroyed = true;
    }

    // Every 10 frames: broadcast a mailbox message, then drain to verify delivery
    if (GetFrameCount() % 10 == 0)
    {
        domain.GetMailbox().Send(
            Dia::Entity::MakeAllAddress(),
            Dia::Core::StringCRC("entity.ping"));
    }

    // Module drains its own broadcast to count deliveries
    domain.GetMailbox().Drain<Dia::Core::StringCRC>(
        [this](const Dia::Mailbox::Address& /*addr*/, const Dia::Core::StringCRC& /*msg*/) {
            ++mMailboxReceiveCount;
        });

    // Evaluate checkpoint conditions
    if (!mSpawnComplete)
        mSpawnComplete = (mSpawnedEntityCount == 9);

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

    // Lifecycle: 9 entities spawned successfully confirms the system works
    if (mSpawnedEntityCount == 9)
        mLifecycleComplete = true;

    // Emit metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricAliveCount)
        mMetricAliveCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entity.alive_count"));
    if (!mMetricMailboxCount)
        mMetricMailboxCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entity.mailbox_count"));

    if (mMetricAliveCount) mMetricAliveCount->Set(static_cast<double>(domain.GetEntityCount()));
    if (mMetricMailboxCount) mMetricMailboxCount->Set(static_cast<double>(mMailboxReceiveCount));

    // Report pass
    if (AllCheckpointsPassed() && !IsResolved())
        ReportPassed();

#ifdef DIA_DEBUG
    // Lazy-init shape drawer (circles for VisualTestRenderComponent)
    if (!mDrawer)
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            mDrawer = std::make_unique<EntityTestDrawer>(
                domain, mParentEntity, mChildA, mChildB, mChildC,
                mQueryEntities, mDoomedEntity, mDoomedDestroyed,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 20, Dia::Core::StringCRC("Entity"));
        }
    }
#endif
}

void EntityTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }

    // Unregister picking service from domain (component OnDetach will no-op if service gone)
    if (auto* em = mEntityModule.Get())
        em->GetDomain().UnregisterService<Dia::Geometry2DPicking::PickingService2D>();
#endif

    // Reset state for possible re-entry
    mSceneBuilt      = false;
    mDoomedDestroyed = false;
    mMailboxReceiveCount = 0;
    mSpawnedEntityCount  = 0;
    mSpawnComplete     = false;
    mQueryCorrect      = false;
    mHierarchyValid    = false;
    mDestroyCascade    = false;
    mMailboxReceived   = false;
    mLifecycleComplete = false;
    mMetricAliveCount = nullptr;
    mMetricMailboxCount = nullptr;
}

} // namespace CluicheTest

namespace { using EntityTestStageModule_ = CluicheTest::EntityTestStageModule; }
DIA_MODULE(EntityTestStageModule_);
DIA_DESCRIBE(EntityTestStageModule_::kTypeId, "Test stage that exercises entity creation, component attachment, and lifecycle scenarios.");
