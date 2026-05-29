////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DGridDrawer.h
// Description: IVisualDebugger that draws a world-space grid with coordinate
//              labels. Grid spacing is determined dynamically from the viewport
//              bounds (power-of-10 spacing targeting ~5 lines per axis).
// Feature spec: docs/specs/features/dia/diavisualdebugger/coord2d-debug-overlay.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord2DGridDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord2DGridDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
