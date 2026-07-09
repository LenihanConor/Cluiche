////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DOriginDrawer.h
// Description: IVisualDebugger that draws an RGB axis crosshair at world origin (0,0,0).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord3DOriginDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord3DOriginDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
