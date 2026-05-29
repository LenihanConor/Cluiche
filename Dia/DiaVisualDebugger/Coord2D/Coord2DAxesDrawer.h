////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DAxesDrawer.h
// Description: IVisualDebugger that draws coloured X/Y axis lines spanning
//              the full viewport. Skips an axis if world origin is off-screen.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord2DAxesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord2DAxesDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
