////////////////////////////////////////////////////////////////////////////////
// Filename: IK2DDebugDomain.h
// Description: IDebugDomain implementation for the 2D inverse kinematics
//              subsystem. Owns and bulk-registers the four IK debug drawers
//              and bridges their state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug { class DebugLayerManager; }
namespace Dia::IK2D  { class IKSolver; }
namespace Dia::Rig2D { class Skeleton; }

namespace Dia::IK2D
{
    class IKChainBonesDrawer;
    class IKChainJointsDrawer;
    class IKChainArrowsDrawer;
    class IKReachCirclesDrawer;
}

namespace Dia::IK2D
{

class IK2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 4;

    IK2DDebugDomain(const IKSolver&             solver,
                    const Dia::Rig2D::Skeleton&  skeleton);
    ~IK2DDebugDomain() override;

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

    const IKSolver&                mSolver;
    const Dia::Rig2D::Skeleton&    mSkeleton;
    Dia::Debug::DebugLayerManager* mLayerManager = nullptr;

    std::unique_ptr<IKChainBonesDrawer>    mChainBonesDrawer;
    std::unique_ptr<IKChainJointsDrawer>   mChainJointsDrawer;
    std::unique_ptr<IKChainArrowsDrawer>   mChainArrowsDrawer;
    std::unique_ptr<IKReachCirclesDrawer>  mReachCirclesDrawer;
};

} // namespace Dia::IK2D

#endif // DIA_DEBUG
