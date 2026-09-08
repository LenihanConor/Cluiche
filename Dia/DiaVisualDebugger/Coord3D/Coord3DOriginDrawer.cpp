////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DOriginDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord3D/Coord3DOriginDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::Debug
{

Coord3DOriginDrawer::Coord3DOriginDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord3DOriginDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord3DOrigin;
}

void Coord3DOriginDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("coord3d.origin", ::Dia::Observation::Trace::Category::kDiaGraphics);

    // Arm length: small fraction of the global debug scale
    const float armLength = mManager.GetDebugScale() * 0.1f;

    const Dia::Maths::Vector3D origin(0.0f, 0.0f, 0.0f);

    draw.RequestDrawRay3D(origin, Dia::Maths::Vector3D(1.0f, 0.0f, 0.0f), armLength,
                          Dia::Debug::DebugColourPalette::kError);    // X = red
    draw.RequestDrawRay3D(origin, Dia::Maths::Vector3D(0.0f, 1.0f, 0.0f), armLength,
                          Dia::Debug::DebugColourPalette::kHealthy);  // Y = green
    draw.RequestDrawRay3D(origin, Dia::Maths::Vector3D(0.0f, 0.0f, 1.0f), armLength,
                          Dia::Debug::DebugColourPalette::kGoal);     // Z = blue/cyan
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
