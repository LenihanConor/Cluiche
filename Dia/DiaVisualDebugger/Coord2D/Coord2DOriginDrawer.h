////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DOriginDrawer.h
// Description: IVisualDebugger that draws a crosshair at world origin (0,0).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord2DOriginDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord2DOriginDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
