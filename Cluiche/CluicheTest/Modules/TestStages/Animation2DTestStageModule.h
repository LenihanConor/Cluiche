#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/Animation2DTestDrawer.h"
#include "Modules/VisualDebuggerModule.h"
#endif

namespace CluicheTest {

class Animation2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaAnimation2D: 7-bone dragon, idle->flap_up->flap_down, hierarchical tip pose";
    explicit Animation2DTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~Animation2DTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 240; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    bool LoadAssets();
    void AdvanceToNextClip();
    bool VerifyGoldenPose() const;
    void EmitMetrics();

    static constexpr unsigned int kClipCount = 3;
    static constexpr float kGoldenWingRotation = 0.7853982f; // π/4 — full downstroke
    static constexpr float kPoseTolerance = 0.001f;

    Dia::Rig2D::SkeletonDef mSkeletonDef;
    std::unique_ptr<Dia::Rig2D::Skeleton> mSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose> mPose;

    Dia::Animation2D::AnimClipDef mClipDefs[kClipCount];
    std::unique_ptr<Dia::Animation2D::AnimClip> mClips[kClipCount];
    Dia::Animation2D::AnimClipPlayer mPlayer;

    int mCurrentClipIndex = 0;
    unsigned int mClipsPlayed = 0;
    unsigned int mTotalPlaybackFrames = 0;
    bool mAllCompleted = false;
    bool mPoseCorrect = false;
    int mLingerFrames = 0;   // counts down after pass before auto-exit

    static constexpr int kLingerFrames = 90; // 3s at 30Hz

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Animation2DTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
