////////////////////////////////////////////////////////////////////////////////
// Filename: ShapeLabelsDrawer.h
// Description: IVisualDebugger that draws world-space text labels submitted
//              per-frame by the caller. Clears its buffer at start of Draw().
// Feature spec: docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Core  { class IDebugContext; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// ShapeLabelsDrawer
//
// Submit world-space text labels via SubmitLabel() before each Draw() call.
// Draw() flushes the buffer via RequestDrawText().
//
// Layer: LayerNames::kGeoLabels   Priority: 50
////////////////////////////////////////////////////////////////////////////////
class ShapeLabelsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int   kMaxLabels   = 64;
    static constexpr float kDefaultFontSize = 12.0f;

    explicit ShapeLabelsDrawer(const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    void SubmitLabel(Dia::Maths::Vector2D position, const char* text);

private:
    struct LabelEntry
    {
        float x, y;
        const char* text;
    };

    Dia::Core::Containers::DynamicArrayC<LabelEntry, kMaxLabels> mPending;
    const Dia::Core::IDebugContext& mManager;
    float mFontScale = 1.0f;
};

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
