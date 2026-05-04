////////////////////////////////////////////////////////////////////////////////
// Filename: SoftAnchorLinksDrawer.h
// Description: IVisualDebugger that draws lines from rope endpoint particles to
//              their rigid-body anchor world positions.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::SoftBody2D
{

class SoftAnchorLinksDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftAnchorLinksDrawer(const SoftBodyWorld&                world,
                          const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const SoftBodyWorld&                 mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
