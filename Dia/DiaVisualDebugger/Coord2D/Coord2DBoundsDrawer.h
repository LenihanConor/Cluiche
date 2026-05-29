////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DBoundsDrawer.h
// Description: IVisualDebugger that labels each viewport corner with its
//              world-space coordinates, giving an at-a-glance read of the
//              visible world region.
// Feature spec: docs/specs/features/dia/diavisualdebugger/coord2d-debug-overlay.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord2DBoundsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord2DBoundsDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
