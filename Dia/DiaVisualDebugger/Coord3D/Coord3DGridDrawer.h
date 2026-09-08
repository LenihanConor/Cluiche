////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DGridDrawer.h
// Description: IVisualDebugger that draws an XZ ground-plane grid in 3D world space.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord3DGridDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord3DGridDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
