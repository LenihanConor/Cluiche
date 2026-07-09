////////////////////////////////////////////////////////////////////////////////
// Filename: MeshBoundsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaMesh3DVisualDebugger/MeshBoundsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>

namespace Dia { namespace Mesh3D {

MeshBoundsDrawer::MeshBoundsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                   const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler)
    : mFrameData(frameData)
    , mAssetHandler(assetHandler)
{}

Dia::Core::StringCRC MeshBoundsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kMesh3DBounds;
}

void MeshBoundsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(draw);

    // Unit-cube bounds used as placeholder when asset is unavailable
    static const Dia::Maths::Vector3D kUnitMin(-0.5f, -0.5f, -0.5f);
    static const Dia::Maths::Vector3D kUnitMax( 0.5f,  0.5f,  0.5f);

    const auto& draws = mFrameData.GetMeshDraws();
    const uint32_t count = draws.Size();

    for (uint32_t i = 0; i < count; ++i)
    {
        const Dia::Graphics3D::Mesh3DDrawCommand& cmd = draws[i];

        // Determine local bounds and colour from asset load state
        Dia::Maths::Vector3D localMin = kUnitMin;
        Dia::Maths::Vector3D localMax = kUnitMax;
        Dia::Graphics::RGBA  colour   = Dia::Debug::DebugColourPalette::kInactive;

        const Dia::Mesh3D::Mesh3DAsset* asset = mAssetHandler.LookupMesh(cmd.meshId);
        if (asset != nullptr)
        {
            switch (asset->GetState())
            {
                case Dia::Mesh3D::Mesh3DAsset::State::Ready:
                {
                    const Dia::Geometry3D::AABB& bounds = asset->GetBounds();
                    localMin = bounds.GetMin();
                    localMax = bounds.GetMax();
                    colour   = Dia::Debug::DebugColourPalette::kHealthy;
                    break;
                }
                case Dia::Mesh3D::Mesh3DAsset::State::Pending:
                {
                    colour = Dia::Debug::DebugColourPalette::kWarning;
                    break;
                }
                case Dia::Mesh3D::Mesh3DAsset::State::Failed:
                {
                    colour = Dia::Debug::DebugColourPalette::kError;
                    break;
                }
                default:
                    break;
            }
        }

        // Transform all 8 local AABB corners through the full world matrix.
        // This correctly handles scale, rotation, and translation.
        const Dia::Maths::Matrix44& m = cmd.transform;
        const Dia::Maths::Vector3D c0 = m.TransformPoint(Dia::Maths::Vector3D(localMin.X(), localMin.Y(), localMin.Z()));
        const Dia::Maths::Vector3D c1 = m.TransformPoint(Dia::Maths::Vector3D(localMax.X(), localMin.Y(), localMin.Z()));
        const Dia::Maths::Vector3D c2 = m.TransformPoint(Dia::Maths::Vector3D(localMin.X(), localMax.Y(), localMin.Z()));
        const Dia::Maths::Vector3D c3 = m.TransformPoint(Dia::Maths::Vector3D(localMax.X(), localMax.Y(), localMin.Z()));
        const Dia::Maths::Vector3D c4 = m.TransformPoint(Dia::Maths::Vector3D(localMin.X(), localMin.Y(), localMax.Z()));
        const Dia::Maths::Vector3D c5 = m.TransformPoint(Dia::Maths::Vector3D(localMax.X(), localMin.Y(), localMax.Z()));
        const Dia::Maths::Vector3D c6 = m.TransformPoint(Dia::Maths::Vector3D(localMin.X(), localMax.Y(), localMax.Z()));
        const Dia::Maths::Vector3D c7 = m.TransformPoint(Dia::Maths::Vector3D(localMax.X(), localMax.Y(), localMax.Z()));

        // 4 bottom edges (z = min)
        dbg.RequestDrawLine3D(c0, c1, colour);
        dbg.RequestDrawLine3D(c1, c3, colour);
        dbg.RequestDrawLine3D(c3, c2, colour);
        dbg.RequestDrawLine3D(c2, c0, colour);

        // 4 top edges (z = max)
        dbg.RequestDrawLine3D(c4, c5, colour);
        dbg.RequestDrawLine3D(c5, c7, colour);
        dbg.RequestDrawLine3D(c7, c6, colour);
        dbg.RequestDrawLine3D(c6, c4, colour);

        // 4 vertical edges
        dbg.RequestDrawLine3D(c0, c4, colour);
        dbg.RequestDrawLine3D(c1, c5, colour);
        dbg.RequestDrawLine3D(c2, c6, colour);
        dbg.RequestDrawLine3D(c3, c7, colour);
    }
}

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
