#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class IK2DTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    IK2DTestDrawer(
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
        const Dia::Debug::DebugLayerManager& manager);

    void UpdateSolver(const Dia::IK2D::IKSolver& solver, const Dia::Rig2D::Skeleton& skeleton);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    const Dia::IK2D::IKSolver*           mSolver;
    const Dia::Rig2D::Skeleton*          mSkeleton;
    const int&                           mPhase;
    const int&                           mPhaseFrame;
    const Dia::Maths::Vector2D&          mCurrentTarget;
    const bool&                          mTwoBoneConverged;
    const bool&                          mFABRIKConverged;
    const bool&                          mLookAtAccurate;
    const float&                         mTwoBoneError;
    const float&                         mFABRIKError;
    const float&                         mLookAtError;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
