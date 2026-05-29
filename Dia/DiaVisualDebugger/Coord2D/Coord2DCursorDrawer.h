////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DCursorDrawer.h
// Description: IVisualDebugger that draws a crosshair and world-space label
//              at the current mouse cursor position.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord2DCursorDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord2DCursorDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
