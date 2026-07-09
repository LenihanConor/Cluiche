////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DGridDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord3D/Coord3DGridDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

#include <cmath>

namespace Dia::Debug
{

Coord3DGridDrawer::Coord3DGridDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord3DGridDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord3DGrid;
}

void Coord3DGridDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("coord3d.grid", ::Dia::Observation::Trace::Category::kDiaGraphics);

    // Half-extent: twice the global debug scale — gives a reasonable grid around origin.
    const float halfExtent = mManager.GetDebugScale() * 2.0f;
    if (halfExtent <= 0.0f)
        return;

    // Power-of-10 spacing: largest where >= 4 lines fit per axis.
    const float rawSpacing = halfExtent / 4.0f;
    float spacing = powf(10.0f, floorf(log10f(rawSpacing)));
    if (spacing <= 0.0f)
        return;

    static const int kMaxLines = 200;

    const int startN = static_cast<int>(floorf(-halfExtent / spacing));
    const int endN   = static_cast<int>(floorf( halfExtent / spacing)) + 1;

    int lineCount = 0;

    // Lines along Z (varying X)
    for (int n = startN; n <= endN && lineCount < kMaxLines; ++n)
    {
        const float x = n * spacing;
        if (x < -halfExtent || x > halfExtent)
            continue;
        draw.RequestDrawLine3D(
            Dia::Maths::Vector3D(x, 0.0f, -halfExtent),
            Dia::Maths::Vector3D(x, 0.0f,  halfExtent),
            Dia::Debug::DebugColourPalette::kInactive);
        ++lineCount;
    }

    // Lines along X (varying Z)
    for (int n = startN; n <= endN && lineCount < kMaxLines; ++n)
    {
        const float z = n * spacing;
        if (z < -halfExtent || z > halfExtent)
            continue;
        draw.RequestDrawLine3D(
            Dia::Maths::Vector3D(-halfExtent, 0.0f, z),
            Dia::Maths::Vector3D( halfExtent, 0.0f, z),
            Dia::Debug::DebugColourPalette::kInactive);
        ++lineCount;
    }
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
