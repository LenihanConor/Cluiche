#include "Modules/TestStages/DebugGalleryTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

#ifdef DIA_DEBUG
// Domain headers
#include <DiaGeometry2DVisualDebugger/Geometry2DDebugDomain.h>
#include <DiaAssetRuntimeVisualDebugger/AssetRuntimeDebugDomain.h>
#include <DiaUtilityAIVisualDebugger/UtilityAIDebugDomain.h>
#include <DiaStateMachineVisualDebugger/StateMachineVisualDebugger.h>
#include <DiaBlackboardVisualDebugger/BlackboardVisualDebugger.h>
#include <DiaRulesVisualDebugger/RulesVisualDebugger.h>
#include <DiaRigidBody2DVisualDebugger/RigidBody2DDebugDomain.h>
#include <DiaSoftBody2DVisualDebugger/SoftBody2DDebugDomain.h>
#include <DiaLighting3DVisualDebugger/Lighting3DDebugDomain.h>
#include <DiaIK2DVisualDebugger/IK2DDebugDomain.h>
#include <DiaRig2DVisualDebugger/Rig2DDebugDomain.h>
#include <DiaAnimation2DVisualDebugger/Animation2DDebugDomain.h>
#include <DiaScene2DVisualDebugger/Scene2DDebugDomain.h>
#include <DiaEntityVisualDebugger/EntityDebugDomain.h>
#include <DiaMesh3DVisualDebugger/Mesh3DDebugDomain.h>
#include <DiaSteeringVisualDebugger/SteeringVisualDebugger.h>
#include <DiaPathfindingVisualDebugger/PathfindingVisualDebugger.h>
#include <DiaFlowFieldVisualDebugger/FlowFieldVisualDebugger.h>
#include <DiaHTNVisualDebugger/HTNVisualDebugger.h>
#include <DiaAIBudgetVisualDebugger/AIBudgetVisualDebugger.h>
#include <DiaMailboxVisualDebugger/MailboxVisualDebugger.h>
#include <DiaBehaviourTreeVisualDebugger/BehaviourTreeVisualDebugger.h>
// Fixture headers
#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaRules/RuleSetComponent.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/World/WorldDef.h>
#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaEntity/Domain.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaSteering/SteeringSystem.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/PathResult.h>
#include <DiaFlowField/FlowField.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#endif

#ifdef DIA_DEBUG
namespace
{
    struct GalleryMachineCtx {};

    // Owns the context + machine in a single allocation so unique_ptr<IStateMachineInspectable>
    // in the header can manage the lifetime cleanly.
    class GalleryStateMachine : public Dia::StateMachine::IStateMachineInspectable
    {
    public:
        GalleryStateMachine()
            : mMachine(
                Dia::Core::StringCRC("gallery_sm"),
                Dia::StateMachine::StateMachineBuilder()
                    .State(Dia::Core::StringCRC("idle"))
                    .InitialState(Dia::Core::StringCRC("idle"))
                    .Transition(Dia::Core::StringCRC("active"), Dia::Core::StringCRC("activate"))
                    .State(Dia::Core::StringCRC("active"))
                    .Transition(Dia::Core::StringCRC("idle"), Dia::Core::StringCRC("deactivate"))
                    .Build(),
                mCtx)
        {}

        Dia::Core::StringCRC GetMachineId() const override       { return mMachine.GetMachineId(); }
        Dia::Core::StringCRC GetCurrentStateId() const override  { return mMachine.GetCurrentStateId(); }
        void GetAllStates(Dia::Core::Containers::DynamicArrayC<Dia::StateMachine::StateInfo, 64>& out) const override
            { mMachine.GetAllStates(out); }
        void GetAllTransitions(Dia::Core::Containers::DynamicArrayC<Dia::StateMachine::TransitionInfo, 64>& out) const override
            { mMachine.GetAllTransitions(out); }
        void GetTransitionHistory(Dia::Core::Containers::DynamicArrayC<Dia::StateMachine::TransitionRecord, 32>& out) const override
            { mMachine.GetTransitionHistory(out); }
        void SetTransitionListener(Dia::StateMachine::ITransitionListener* listener) override
            { mMachine.SetTransitionListener(listener); }

    private:
        GalleryMachineCtx mCtx;
        Dia::StateMachine::FlatStateMachine<GalleryMachineCtx> mMachine;
    };
}
#endif

namespace CluicheTest {

const Dia::Core::StringCRC DebugGalleryTestStageModule::kTypeId("DebugGalleryTestStageModule");

DebugGalleryTestStageModule::DebugGalleryTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

DebugGalleryTestStageModule::~DebugGalleryTestStageModule() = default;

Dia::Core::StringCRC DebugGalleryTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("DebugGalleryTestStage");
}

const Dia::Core::StringCRC* DebugGalleryTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.debuggallery.passed")
    };
    outCount = 1;
    return names;
}

void DebugGalleryTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.debuggallery.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            const bool passed = GetFrameCount() >= 300;
            return { passed, passed ? "300 frames elapsed" : "pending", 0.0f };
        });

#ifdef DIA_DEBUG
    auto* vd = mVisualDebuggerRef.Get();
    if (!vd)
    {
        DIA_LOG_WARNING("CluicheTest", "DebugGalleryTestStageModule — VisualDebuggerModule unavailable, skipping domain registration");
        return;
    }

    // Build shared rig — 5-bone straight-line limb used by IK2D, Rig2D, and Animation2D domains.
    {
        auto skelDef   = Dia::IK2D::Testing::MakeLimbSkeletonDef(5, 0.8f);
        mSkeleton      = std::make_unique<Dia::Rig2D::Skeleton>(*skelDef);
        mPose          = std::make_unique<Dia::Rig2D::Pose>(*mSkeleton);
        mPose->SetToBindPose(*mSkeleton);
        mIKSolver      = std::make_unique<Dia::IK2D::IKSolver>(*mSkeleton, *mPose);
        mIKSolver->SetRootTransform(Dia::Rig2D::BoneTransform{});
        mAnimEvaluator = std::make_unique<Dia::Animation2D::AnimationEvaluator>(*mSkeleton);
    }

    // Physics worlds
    {
        Dia::RigidBody2D::WorldDef def;
        mPhysicsWorld = std::make_unique<Dia::RigidBody2D::PhysicsWorld>(def);
    }
    {
        Dia::SoftBody2D::WorldDef def;
        mSoftBodyWorld = std::make_unique<Dia::SoftBody2D::SoftBodyWorld>(def);
    }

    // Remaining fixtures — all default-constructible
    mUtilitySet       = std::make_unique<Dia::UtilityAI::UtilitySet>();
    mLightRegistry3D  = std::make_unique<Dia::Lighting3D::LightRegistry3D>();
    mCameraRegistry   = std::make_unique<Dia::Camera2D::CameraRegistry2D>();
    mLightRegistry2D  = std::make_unique<Dia::Lighting2D::LightRegistry2D>();
    mLayerTable       = std::make_unique<Dia::Scene2D::LayerTable>();
    mEntityDomain     = std::make_unique<Dia::Entity::Domain>();
    mMeshFrameData    = std::make_unique<Dia::Graphics3D::Mesh3DFrameData>();
    mMeshAssetHandler  = std::make_unique<Dia::Mesh3D::Mesh3DAssetHandler>();
    mSteeringSystem    = std::make_unique<Dia::Steering::SteeringSystem>();
    mPathGrid          = std::make_unique<Dia::Pathfinding::SquarePathGrid>(8, 8);
    mPathResult        = std::make_unique<Dia::Pathfinding::PathResult>();
    mFlowField         = std::make_unique<Dia::FlowField::FlowField>(8, 8);
    mHTNPlanner        = std::make_unique<Dia::HTN::HTNPlannerComponent>();
    mAIBudgetScheduler = std::make_unique<Dia::AIBudget::AIBudgetScheduler>();
    mAIBudgetResult    = std::make_unique<Dia::AIBudget::AIBudgetResult>();
    mMailbox           = std::make_unique<Dia::Mailbox::Mailbox>();
    mStateMachine      = std::make_unique<GalleryStateMachine>();
    mBlackboard       = std::make_unique<Dia::Blackboard::Blackboard>();
    mRuleSetComponent = std::make_unique<Dia::Rules::RuleSetComponent>();

    // Construct all 15 gallery domains
    mGeometry2DDomain   = std::make_unique<Dia::Geometry2DVisualDebugger::Geometry2DDebugDomain>();
    mAssetRuntimeDomain = std::make_unique<Dia::AssetRuntime::AssetRuntimeDebugDomain>();
    mUtilityAIDomain    = std::make_unique<Dia::UtilityAI::UtilityAIDebugDomain>(*mUtilitySet);
    mRigidBody2DDomain  = std::make_unique<Dia::RigidBody2D::RigidBody2DDebugDomain>(*mPhysicsWorld);
    mSoftBody2DDomain   = std::make_unique<Dia::SoftBody2D::SoftBody2DDebugDomain>(*mSoftBodyWorld);
    mLighting3DDomain   = std::make_unique<Dia::Lighting3D::Lighting3DDebugDomain>(*mLightRegistry3D);
    mIK2DDomain         = std::make_unique<Dia::IK2D::IK2DDebugDomain>(*mIKSolver, *mSkeleton);
    mRig2DDomain        = std::make_unique<Dia::Rig2D::Rig2DDebugDomain>(*mSkeleton, mIKSolver->GetWorldTransforms());
    mAnimation2DDomain  = std::make_unique<Dia::Animation2D::Animation2DDebugDomain>(
                              *mAnimEvaluator, *mSkeleton, mIKSolver->GetWorldTransforms());
    mScene2DDomain      = std::make_unique<Dia::Scene2DVisualDebugger::Scene2DDebugDomain>(
                              *mCameraRegistry, *mLightRegistry2D, *mLayerTable);
    mEntityDebugDomain  = std::make_unique<Dia::EntityVisualDebugger::EntityDebugDomain>(
                              *mEntityDomain, *mEntityDomain, Dia::Core::StringCRC("TransformComponent"));
    mMesh3DDomain       = std::make_unique<Dia::Mesh3D::Mesh3DDebugDomain>(*mMeshFrameData, *mMeshAssetHandler);
    mStateMachineDomain = std::make_unique<Dia::StateMachine::StateMachineVisualDebugger>(*mStateMachine);
    mBlackboardDomain   = std::make_unique<Dia::Blackboard::BlackboardVisualDebugger>(*mBlackboard);
    mRulesDomain        = std::make_unique<Dia::Rules::RulesVisualDebugger>(*mRuleSetComponent);
    mSteeringDomain     = std::make_unique<Dia::Steering::SteeringVisualDebugger>(*mSteeringSystem);
    mPathfindingDomain  = std::make_unique<Dia::Pathfinding::PathfindingVisualDebugger>(*mPathGrid, *mPathResult, 1.0f);
    mFlowFieldDomain    = std::make_unique<Dia::FlowField::FlowFieldVisualDebugger>(*mFlowField, 1.0f);
    mHTNDomain          = std::make_unique<Dia::HTN::HTNVisualDebugger>(*mHTNPlanner);
    mAIBudgetDomain     = std::make_unique<Dia::AIBudget::AIBudgetVisualDebugger>(*mAIBudgetScheduler, *mAIBudgetResult);
    mMailboxDomain      = std::make_unique<Dia::Mailbox::MailboxVisualDebugger>(*mMailbox);
    mBTComponent        = std::make_unique<Dia::BehaviourTree::BehaviourTreeComponent>();
    mBehaviourTreeDomain = std::make_unique<Dia::BehaviourTree::BehaviourTreeVisualDebugger>(*mBTComponent);

    DIA_LOG_INFO("CluicheTest", "DebugGalleryTestStageModule — fixtures built; domains register on first update");
#endif
}

void DebugGalleryTestStageModule::OnUpdate(float /*deltaTime*/)
{
#ifdef DIA_DEBUG
    if (!mDomainsRegistered)
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            vd->RegisterDomain(*mGeometry2DDomain);
            vd->RegisterDomain(*mAssetRuntimeDomain);
            vd->RegisterDomain(*mUtilityAIDomain);
            vd->RegisterDomain(*mRigidBody2DDomain);
            vd->RegisterDomain(*mSoftBody2DDomain);
            vd->RegisterDomain(*mLighting3DDomain);
            vd->RegisterDomain(*mIK2DDomain);
            vd->RegisterDomain(*mRig2DDomain);
            vd->RegisterDomain(*mAnimation2DDomain);
            vd->RegisterDomain(*mScene2DDomain);
            vd->RegisterDomain(*mEntityDebugDomain);
            vd->RegisterDomain(*mMesh3DDomain);
            vd->RegisterDomain(*mStateMachineDomain);
            vd->RegisterDomain(*mBlackboardDomain);
            vd->RegisterDomain(*mRulesDomain);
            vd->RegisterDomain(*mSteeringDomain);
            vd->RegisterDomain(*mPathfindingDomain);
            vd->RegisterDomain(*mFlowFieldDomain);
            vd->RegisterDomain(*mHTNDomain);
            vd->RegisterDomain(*mAIBudgetDomain);
            vd->RegisterDomain(*mMailboxDomain);
            vd->RegisterDomain(*mBehaviourTreeDomain);
            mDomainsRegistered = true;
            DIA_LOG_INFO("CluicheTest", "DebugGalleryTestStageModule — 22 domains registered");
        }
    }
#endif

    if (!IsResolved() && GetFrameCount() >= 300)
        ReportPassed();
}

void DebugGalleryTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    auto* vd = mVisualDebuggerRef.Get();
    if (vd)
    {
        if (mBehaviourTreeDomain) vd->UnregisterDomain(*mBehaviourTreeDomain);
        if (mMailboxDomain)       vd->UnregisterDomain(*mMailboxDomain);
        if (mAIBudgetDomain)      vd->UnregisterDomain(*mAIBudgetDomain);
        if (mHTNDomain)           vd->UnregisterDomain(*mHTNDomain);
        if (mFlowFieldDomain)     vd->UnregisterDomain(*mFlowFieldDomain);
        if (mPathfindingDomain)   vd->UnregisterDomain(*mPathfindingDomain);
        if (mSteeringDomain)      vd->UnregisterDomain(*mSteeringDomain);
        if (mRulesDomain)         vd->UnregisterDomain(*mRulesDomain);
        if (mBlackboardDomain)    vd->UnregisterDomain(*mBlackboardDomain);
        if (mStateMachineDomain)  vd->UnregisterDomain(*mStateMachineDomain);
        if (mMesh3DDomain)        vd->UnregisterDomain(*mMesh3DDomain);
        if (mEntityDebugDomain)   vd->UnregisterDomain(*mEntityDebugDomain);
        if (mScene2DDomain)      vd->UnregisterDomain(*mScene2DDomain);
        if (mAnimation2DDomain)  vd->UnregisterDomain(*mAnimation2DDomain);
        if (mRig2DDomain)        vd->UnregisterDomain(*mRig2DDomain);
        if (mIK2DDomain)         vd->UnregisterDomain(*mIK2DDomain);
        if (mLighting3DDomain)   vd->UnregisterDomain(*mLighting3DDomain);
        if (mSoftBody2DDomain)   vd->UnregisterDomain(*mSoftBody2DDomain);
        if (mRigidBody2DDomain)  vd->UnregisterDomain(*mRigidBody2DDomain);
        if (mUtilityAIDomain)    vd->UnregisterDomain(*mUtilityAIDomain);
        if (mAssetRuntimeDomain) vd->UnregisterDomain(*mAssetRuntimeDomain);
        if (mGeometry2DDomain)   vd->UnregisterDomain(*mGeometry2DDomain);
    }

    // Domains first, then the fixtures they reference.
    mBehaviourTreeDomain.reset();
    mMailboxDomain.reset();
    mAIBudgetDomain.reset();
    mHTNDomain.reset();
    mFlowFieldDomain.reset();
    mPathfindingDomain.reset();
    mSteeringDomain.reset();
    mRulesDomain.reset();
    mBlackboardDomain.reset();
    mStateMachineDomain.reset();
    mMesh3DDomain.reset();
    mEntityDebugDomain.reset();
    mScene2DDomain.reset();
    mAnimation2DDomain.reset();
    mRig2DDomain.reset();
    mIK2DDomain.reset();
    mLighting3DDomain.reset();
    mSoftBody2DDomain.reset();
    mRigidBody2DDomain.reset();
    mUtilityAIDomain.reset();
    mAssetRuntimeDomain.reset();
    mGeometry2DDomain.reset();

    mMeshAssetHandler.reset();
    mMeshFrameData.reset();
    mEntityDomain.reset();
    mLayerTable.reset();
    mLightRegistry2D.reset();
    mCameraRegistry.reset();
    mAnimEvaluator.reset();
    mIKSolver.reset();
    mPose.reset();
    mSkeleton.reset();
    mLightRegistry3D.reset();
    mRuleSetComponent.reset();
    mBlackboard.reset();
    mStateMachine.reset();
    mUtilitySet.reset();
    mSoftBodyWorld.reset();
    mPhysicsWorld.reset();
    mMailbox.reset();
    mAIBudgetResult.reset();
    mAIBudgetScheduler.reset();
    mHTNPlanner.reset();
    mFlowField.reset();
    mPathResult.reset();
    mPathGrid.reset();
    mSteeringSystem.reset();
    mBTComponent.reset();

    mDomainsRegistered = false;
    DIA_LOG_INFO("CluicheTest", "DebugGalleryTestStageModule — 22 domains unregistered");
#endif
}

} // namespace CluicheTest

namespace { using DebugGalleryTestStageModule_ = CluicheTest::DebugGalleryTestStageModule; }
DIA_MODULE(DebugGalleryTestStageModule_);
DIA_DESCRIBE(DebugGalleryTestStageModule_::kTypeId,
    "Visual gallery stage: registers all 24 IDebugDomain instances simultaneously for DiaDebugPanel validation.");
