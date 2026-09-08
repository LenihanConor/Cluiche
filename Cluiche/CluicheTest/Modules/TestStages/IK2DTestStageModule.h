#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/IK2DTestDrawer.h"
#include "Modules/VisualDebuggerModule.h"
#endif

namespace CluicheTest {

// Phase 1 (two-bone) and Phase 2 (FABRIK) use a procedural straight-line limb
// skeleton built via IKTestHelpers::BuildLimbSkeletonDef. The dragon skeleton's
// wing bones are non-consecutive (Root=0, Wing_R=4, WingTip_R=6), which is
// incompatible with IKSolver's midIdx = startIdx+1 assumption.
// Phase 3 (look-at) uses dragon.diarig — bones are addressed by name not index.

class IK2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Validates DiaIK2D: two-bone (5-bone limb), FABRIK (5-bone limb), look-at head on dragon skeleton";
    explicit IK2DTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~IK2DTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    bool LoadDragonAsset();
    void BuildLimbRig();
    void RegisterChains();
    Dia::Maths::Vector2D ComputePhase1Target(int phaseFrame) const;
    Dia::Maths::Vector2D ComputePhase2Target(int phaseFrame) const;
    Dia::Maths::Vector2D ComputePhase3Target(int phaseFrame) const;
    void VerifyPhase1();
    void VerifyPhase2();
    void VerifyPhase3();
    void EmitMetrics();

    // Phases 1+2: procedural 5-bone straight-line limb (consecutive indices)
    Dia::Rig2D::SkeletonDef                mLimbDef;
    std::unique_ptr<Dia::Rig2D::Skeleton>  mLimbSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose>      mLimbPose;
    std::unique_ptr<Dia::IK2D::IKSolver>   mLimbSolver;

    // Phase 3: dragon skeleton loaded from file (look-at only — no chain needed)
    Dia::Rig2D::SkeletonDef                mDragonDef;
    std::unique_ptr<Dia::Rig2D::Skeleton>  mDragonSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose>      mDragonPose;
    std::unique_ptr<Dia::IK2D::IKSolver>   mDragonSolver;

    int   mPhase      = 0;
    int   mPhaseFrame = 0;
    Dia::Maths::Vector2D mCurrentTarget;

    bool  mTwoBoneConverged = false;
    bool  mFABRIKConverged  = false;
    bool  mLookAtAccurate   = false;
    float mTwoBoneError     = 3.402823466e+38f;
    float mFABRIKError      = 3.402823466e+38f;
    float mLookAtError      = 3.402823466e+38f;

    int mLingerFrames = 0;

    static constexpr int   kLingerFrames      = 60;
    static constexpr int   kPhase1Frames      = 90;
    static constexpr int   kPhase2Frames      = 90;
    static constexpr int   kPhase3Frames      = 60;
    static constexpr int   kPhase1GoldenFrame = 60;
    static constexpr int   kPhase2GoldenFrame = 60;
    static constexpr int   kPhase3GoldenFrame = 30;
    static constexpr float kLimbBoneLength    = 0.8f;
    static constexpr int   kLimbBoneCount     = 5;
    static constexpr float kPositionTolerance = 0.01f;
    static constexpr float kAngleTolerance    = 0.05f;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<IK2DTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
