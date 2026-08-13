////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DCameraDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord3D/Coord3DCameraDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics3D/Camera3D.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::Debug
{

Coord3DCameraDrawer::Coord3DCameraDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord3DCameraDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord3DCamera;
}

void Coord3DCameraDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
    // No geometry — camera state was previously rendered via DrawImGui (now removed).
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
