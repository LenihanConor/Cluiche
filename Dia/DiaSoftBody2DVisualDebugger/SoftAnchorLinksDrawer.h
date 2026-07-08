////////////////////////////////////////////////////////////////////////////////
// Filename: SoftAnchorLinksDrawer.h
// Description: IVisualDebugger that draws lines from rope endpoint particles to
//              their rigid-body anchor world positions.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::SoftBody2D
{

class SoftAnchorLinksDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftAnchorLinksDrawer(const SoftBodyWorld&             world,
                          const Dia::Core::IDebugContext&  manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const SoftBodyWorld&                 mWorld;
    [[maybe_unused]] const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
