#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/IK2DTestDrawer.h"

#include <DiaRig2DVisualDebugger/BoneLinesDrawer.h>
#include <DiaRig2DVisualDebugger/JointCirclesDrawer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <imgui.h>
#include <cmath>

namespace CluicheTest {

static const char* kPhaseNames[] = {
    "Two-Bone (5-bone limb)",
    "FABRIK (5-bone limb)",
    "Look-At (dragon head)",
    "Done"
};

// World units → pixels.  Dragon uses 200 (same as Animation2DTestDrawer).
// Limb uses 100 — 4 bones × 0.8u × 100 = 320px total chain height, readable.
static constexpr float kLimbScale = 100.0f;
static constexpr float kDragScale = 200.0f;
static constexpr float kOriginX   = 400.0f;
static constexpr float kOriginY   = 300.0f;

IK2DTestDrawer::IK2DTestDrawer(
    const Dia::IK2D::IKSolver&           solver,
    const Dia::Rig2D::Skeleton&          skeleton,
    const int&                           phase,
    const int&                           phaseFrame,
    const Dia::Maths::Vector2D&          currentTarget,
    const bool&                          twoBoneConverged,
    const bool&                          fabrikConverged,
    const bool&                          lookAtAccurate,
    const float&                         twoBoneError,
    const float&                         fabrikError,
    const float&                         lookAtError,
    const Dia::Debug::DebugLayerManager& manager)
    : mSolver(&solver)
    , mSkeleton(&skeleton)
    , mPhase(phase)
    , mPhaseFrame(phaseFrame)
    , mCurrentTarget(currentTarget)
    , mTwoBoneConverged(twoBoneConverged)
    , mFABRIKConverged(fabrikConverged)
    , mLookAtAccurate(lookAtAccurate)
    , mTwoBoneError(twoBoneError)
    , mFABRIKError(fabrikError)
    , mLookAtError(lookAtError)
    , mManager(manager)
{}

void IK2DTestDrawer::UpdateSolver(const Dia::IK2D::IKSolver& solver, const Dia::Rig2D::Skeleton& skeleton)
{
    mSolver   = &solver;
    mSkeleton = &skeleton;
}

Dia::Core::StringCRC IK2DTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("ik2d.overview");
}

static Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones>
BuildScreenTransforms(
    const Dia::IK2D::IKSolver& solver,
    float scale)
{
    const auto& wt = solver.GetWorldTransforms();
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> out;
    for (unsigned int i = 0; i < wt.Size(); ++i)
    {
        Dia::Rig2D::BoneTransform st = wt[i];
        st.position = Dia::Maths::Vector2D(
            kOriginX + wt[i].position.X() * scale,
            kOriginY - wt[i].position.Y() * scale);   // Y-flip: math Y-up → screen Y-down
        out.Add(st);
    }
    return out;
}

void IK2DTestDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const float scale = (mPhase < 2) ? kLimbScale : kDragScale;
    auto screenTransforms = BuildScreenTransforms(*mSolver, scale);

    // Bone lines + joint circles work the same for all phases
    Dia::Rig2D::BoneLinesDrawer    boneLines   (*mSkeleton, screenTransforms, mManager);
    Dia::Rig2D::JointCirclesDrawer jointCircles(*mSkeleton, screenTransforms, mManager);
    boneLines.Draw(frameData);
    jointCircles.Draw(frameData);

    if (mPhase < 2)
    {
        // Draw direction arrows parent→child along each IK chain bone
        const int chainCount = mSolver->GetChainCount();
        for (int c = 0; c < chainCount; ++c)
        {
            const int startIdx = mSolver->GetChainStartBoneIndex(c);
            const int endIdx   = mSolver->GetChainEndBoneIndex(c);
            for (int i = startIdx; i < endIdx; ++i)
            {
                const Dia::Maths::Vector2D& from = screenTransforms[i].position;
                const Dia::Maths::Vector2D& to   = screenTransforms[i + 1].position;
                float dx = to.X() - from.X();
                float dy = to.Y() - from.Y();
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 0.001f) continue;
                Dia::Maths::Vector2D dir(dx / len, dy / len);
                frameData.RequestDrawRay(from, dir, len * 0.7f, Dia::Debug::DebugColourPalette::kGoal);
            }

            // Reach circle at chain root
            float reachRadius = 0.0f;
            for (int i = startIdx; i < endIdx; ++i)
                reachRadius += mSkeleton->GetBone(i).length;
            if (reachRadius > 0.0f)
                frameData.RequestDraw(screenTransforms[startIdx].position,
                    reachRadius * scale, Dia::Debug::DebugColourPalette::kInactive);
        }

        // Target marker
        Dia::Maths::Vector2D ts(kOriginX + mCurrentTarget.X() * scale,
                                kOriginY - mCurrentTarget.Y() * scale);
        frameData.RequestDraw(ts, 8.0f, Dia::Debug::DebugColourPalette::kGoal);
    }
    else
    {
        // Phase 2 look-at: draw head→target line
        int headIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Head"));
        if (headIdx >= 0 && static_cast<unsigned int>(headIdx) < screenTransforms.Size())
        {
            Dia::Maths::Vector2D targetScreen(kOriginX + mCurrentTarget.X() * scale,
                                              kOriginY - mCurrentTarget.Y() * scale);
            frameData.RequestDraw(screenTransforms[headIdx].position, targetScreen,
                Dia::Debug::DebugColourPalette::kGoal);
            frameData.RequestDraw(targetScreen, 8.0f, Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

void IK2DTestDrawer::DrawImGui()
{
    int phaseIdx = (mPhase < 4) ? mPhase : 3;
    ImGui::Text("IK2D Test Stage");
    ImGui::Separator();
    ImGui::Text("Phase: %s (frame %d)", kPhaseNames[phaseIdx], mPhaseFrame);
    ImGui::Text("Target: (%.2f, %.2f)", mCurrentTarget.X(), mCurrentTarget.Y());
    ImGui::Separator();

    auto resultLine = [](const char* label, bool passed, float error, const char* unit) {
        if (error < 1e+10f)
            ImGui::Text("%s: %s  err=%.4f%s", label, passed ? "PASS" : "FAIL", error, unit);
        else
            ImGui::Text("%s: pending", label);
    };
    resultLine("Two-Bone", mTwoBoneConverged, mTwoBoneError, "u");
    resultLine("FABRIK",   mFABRIKConverged,  mFABRIKError,  "u");
    resultLine("Look-At",  mLookAtAccurate,   mLookAtError,  "rad");
}

} // namespace CluicheTest

#endif // DIA_DEBUG
