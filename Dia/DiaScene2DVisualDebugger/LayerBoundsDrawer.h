////////////////////////////////////////////////////////////////////////////////
// Filename: LayerBoundsDrawer.h
// Description: IVisualDebugger that draws Scene2D layer band rectangles and
//              the world bounds outline.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::Scene2D { class LayerTable; }
namespace Dia::Core    { class IDebugContext; }

namespace Dia::Scene2DVisualDebugger
{

class LayerBoundsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    LayerBoundsDrawer(const Dia::Scene2D::LayerTable& layerTable,
                      const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::Scene2D::LayerTable& mLayerTable;
    const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
