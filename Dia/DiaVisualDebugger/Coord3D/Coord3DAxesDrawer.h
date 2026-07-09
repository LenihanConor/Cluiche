////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DAxesDrawer.h
// Description: IVisualDebugger that draws full X/Y/Z axis lines in 3D world space.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord3DAxesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord3DAxesDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
    float mExtent = 100.0f;  // world units each direction along each axis
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
