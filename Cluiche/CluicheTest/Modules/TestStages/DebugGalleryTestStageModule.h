#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <memory>

// Forward declarations — full headers pulled in DebugGalleryTestStageModule.cpp
// (destructor defined there so unique_ptr can destroy complete types).
namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::SoftBody2D  { class SoftBodyWorld; }
namespace Dia::UtilityAI   { class UtilitySet; }
namespace Dia::Lighting3D  { class LightRegistry3D; }
namespace Dia::Rig2D       { class Skeleton; class Pose; }
namespace Dia::IK2D        { class IKSolver; }
namespace Dia::Animation2D { class AnimationEvaluator; }
namespace Dia::Camera2D    { class CameraRegistry2D; }
namespace Dia::Lighting2D  { class LightRegistry2D; }
namespace Dia::Scene2D     { class LayerTable; }
namespace Dia::Entity      { class Domain; }
namespace Dia::Graphics3D  { class Mesh3DFrameData; }
namespace Dia::Mesh3D      { class Mesh3DAssetHandler; }

namespace Dia::Geometry2DVisualDebugger { class Geometry2DDebugDomain; }
namespace Dia::AssetRuntime             { class AssetRuntimeDebugDomain; }
namespace Dia::UtilityAI               { class UtilityAIDebugDomain; }
namespace Dia::RigidBody2D             { class RigidBody2DDebugDomain; }
namespace Dia::SoftBody2D              { class SoftBody2DDebugDomain; }
namespace Dia::Lighting3D              { class Lighting3DDebugDomain; }
namespace Dia::IK2D                    { class IK2DDebugDomain; }
namespace Dia::Rig2D                   { class Rig2DDebugDomain; }
namespace Dia::Animation2D             { class Animation2DDebugDomain; }
namespace Dia::Scene2DVisualDebugger   { class Scene2DDebugDomain; }
namespace Dia::EntityVisualDebugger    { class EntityDebugDomain; }
namespace Dia::Mesh3D                  { class Mesh3DDebugDomain; }
namespace Dia::StateMachine            { class IStateMachineInspectable; class StateMachineVisualDebugger; }
namespace Dia::Blackboard              { class Blackboard; class BlackboardVisualDebugger; }
#endif

namespace CluicheTest {

class DebugGalleryTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Visual gallery: registers all 16 IDebugDomain instances with synthetic fixture data for panel validation";
    explicit DebugGalleryTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~DebugGalleryTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

#ifdef DIA_DEBUG
private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};

    // Synthetic fixture objects referenced by domain constructors.
    // Owned here; must outlive the domains below.
    std::unique_ptr<Dia::RigidBody2D::PhysicsWorld>       mPhysicsWorld;
    std::unique_ptr<Dia::SoftBody2D::SoftBodyWorld>       mSoftBodyWorld;
    std::unique_ptr<Dia::UtilityAI::UtilitySet>           mUtilitySet;
    std::unique_ptr<Dia::Lighting3D::LightRegistry3D>     mLightRegistry3D;
    std::unique_ptr<Dia::Rig2D::Skeleton>                 mSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose>                     mPose;
    std::unique_ptr<Dia::IK2D::IKSolver>                  mIKSolver;
    std::unique_ptr<Dia::Animation2D::AnimationEvaluator> mAnimEvaluator;
    std::unique_ptr<Dia::Camera2D::CameraRegistry2D>      mCameraRegistry;
    std::unique_ptr<Dia::Lighting2D::LightRegistry2D>     mLightRegistry2D;
    std::unique_ptr<Dia::Scene2D::LayerTable>             mLayerTable;
    std::unique_ptr<Dia::Entity::Domain>                  mEntityDomain;
    std::unique_ptr<Dia::Graphics3D::Mesh3DFrameData>     mMeshFrameData;
    std::unique_ptr<Dia::Mesh3D::Mesh3DAssetHandler>      mMeshAssetHandler;

    bool mDomainsRegistered = false;

    // 14 gallery domains. Coord2DDebugDomain and Coord3DDebugDomain are excluded
    // because VisualDebuggerModule already registers them on startup; registering
    // them again would fire a DIA_ASSERT (duplicate domain ID). The panel sees all
    // 16 domains: 2 from VisualDebuggerModule + 14 from this stage.
    std::unique_ptr<Dia::Geometry2DVisualDebugger::Geometry2DDebugDomain>  mGeometry2DDomain;
    std::unique_ptr<Dia::AssetRuntime::AssetRuntimeDebugDomain>            mAssetRuntimeDomain;
    std::unique_ptr<Dia::UtilityAI::UtilityAIDebugDomain>                  mUtilityAIDomain;
    std::unique_ptr<Dia::RigidBody2D::RigidBody2DDebugDomain>              mRigidBody2DDomain;
    std::unique_ptr<Dia::SoftBody2D::SoftBody2DDebugDomain>                mSoftBody2DDomain;
    std::unique_ptr<Dia::Lighting3D::Lighting3DDebugDomain>                mLighting3DDomain;
    std::unique_ptr<Dia::IK2D::IK2DDebugDomain>                           mIK2DDomain;
    std::unique_ptr<Dia::Rig2D::Rig2DDebugDomain>                         mRig2DDomain;
    std::unique_ptr<Dia::Animation2D::Animation2DDebugDomain>             mAnimation2DDomain;
    std::unique_ptr<Dia::Scene2DVisualDebugger::Scene2DDebugDomain>       mScene2DDomain;
    std::unique_ptr<Dia::EntityVisualDebugger::EntityDebugDomain>         mEntityDebugDomain;
    std::unique_ptr<Dia::Mesh3D::Mesh3DDebugDomain>                       mMesh3DDomain;
    std::unique_ptr<Dia::StateMachine::IStateMachineInspectable>          mStateMachine;
    std::unique_ptr<Dia::Blackboard::Blackboard>                          mBlackboard;
    std::unique_ptr<Dia::StateMachine::StateMachineVisualDebugger>        mStateMachineDomain;
    std::unique_ptr<Dia::Blackboard::BlackboardVisualDebugger>            mBlackboardDomain;
#endif
};

} // namespace CluicheTest
