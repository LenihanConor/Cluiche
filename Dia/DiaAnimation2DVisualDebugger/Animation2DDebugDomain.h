////////////////////////////////////////////////////////////////////////////////
// Filename: Animation2DDebugDomain.h
// Description: IDebugDomain implementation for the 2D animation evaluation
//              subsystem. Owns and bulk-registers the three animation debug
//              drawers and bridges their state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <memory>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Animation2D { class AnimationEvaluator; }
namespace Dia::Rig2D       { class Skeleton; }

namespace Dia::Animation2D
{
    class AnimBlendWeightsDrawer;
    class AnimClipCursorDrawer;
    class AnimSpringDrawer;
}

namespace Dia::Animation2D
{

class Animation2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 3;

    Animation2DDebugDomain(
        const AnimationEvaluator&                                                    evaluator,
        const Dia::Rig2D::Skeleton&                                                  skeleton,
        const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms);
    ~Animation2DDebugDomain() override;

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

    const AnimationEvaluator&                                                    mEvaluator;
    const Dia::Rig2D::Skeleton&                                                  mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& mWorldTransforms;
    Dia::Debug::DebugLayerManager*                                               mLayerManager = nullptr;

    std::unique_ptr<AnimBlendWeightsDrawer> mBlendWeightsDrawer;
    std::unique_ptr<AnimClipCursorDrawer>   mClipCursorDrawer;
    std::unique_ptr<AnimSpringDrawer>       mSpringDrawer;
};

} // namespace Dia::Animation2D

#endif // DIA_DEBUG
