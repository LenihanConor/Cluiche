////////////////////////////////////////////////////////////////////////////////
// Filename: Rig2DDebugDomain.h
// Description: IDebugDomain implementation for the 2D skeleton/rig subsystem.
//              Owns and bulk-registers the five rig debug drawers and bridges
//              their state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <memory>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Rig2D
{
    class BoneLinesDrawer;
    class JointCirclesDrawer;
    class DirectionArrowsDrawer;
    class BoneLabelsDrawer;
    class RestPoseDrawer;
}

namespace Dia::Rig2D
{

class Rig2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 5;

    Rig2DDebugDomain(
        const Skeleton&                                                          skeleton,
        const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>&   worldTransforms);
    ~Rig2DDebugDomain() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return true; }

    // ---- IDebugDomain: lifecycle ----
    void Register(Dia::Debug::DebugLayerManager& mgr)   override;
    void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- IDebugDomain: drawer access ----
    int                          GetDrawerCount() const override;
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

private:
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    const Skeleton&                                                        mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& mWorldTransforms;
    Dia::Debug::DebugLayerManager*                                         mLayerManager = nullptr;

    std::unique_ptr<BoneLinesDrawer>       mBoneLinesDrawer;
    std::unique_ptr<JointCirclesDrawer>    mJointCirclesDrawer;
    std::unique_ptr<DirectionArrowsDrawer> mDirectionArrowsDrawer;
    std::unique_ptr<BoneLabelsDrawer>      mBoneLabelsDrawer;
    std::unique_ptr<RestPoseDrawer>        mRestPoseDrawer;
};

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
