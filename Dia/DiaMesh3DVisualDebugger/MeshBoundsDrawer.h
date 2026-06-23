////////////////////////////////////////////////////////////////////////////////
// Filename: MeshBoundsDrawer.h
// Description: IVisualDebugger that draws wireframe AABB bounds for each
//              Mesh3DDrawCommand in the frame. Ready meshes show their real
//              asset bounds (green); pending and failed meshes show a unit
//              cube placeholder (yellow / red); null-lookup shows grey.
// Feature spec: docs/specs/applications/dia/systems/diamesh3dvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }
namespace Dia { namespace Mesh3D     { class Mesh3DAssetHandler; } }

namespace Dia { namespace Mesh3D {

////////////////////////////////////////////////////////////////////////////////
// MeshBoundsDrawer
//
// Registered under layer name LayerNames::kMesh3DBounds ("mesh3d.bounds").
// Each frame, iterates all Mesh3DDrawCommands and emits 12 wireframe AABB
// edges per command.  Colour encodes asset load state (SD-MVD-002).
////////////////////////////////////////////////////////////////////////////////
class MeshBoundsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    MeshBoundsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                     const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
    const Dia::Mesh3D::Mesh3DAssetHandler&  mAssetHandler;
};

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
