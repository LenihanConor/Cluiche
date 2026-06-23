////////////////////////////////////////////////////////////////////////////////
// Filename: MeshBoundsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaMesh3DVisualDebugger/MeshBoundsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>

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

void MeshBoundsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(frameData);

    // Unit-cube bounds used as placeholder when asset is unavailable
    static const Dia::Maths::Vector3D kUnitMin(-0.5f, -0.5f, -0.5f);
    static const Dia::Maths::Vector3D kUnitMax( 0.5f,  0.5f,  0.5f);

    const auto& draws = mFrameData.GetMeshDraws();
    const uint32_t count = draws.Size();

    for (uint32_t i = 0; i < count; ++i)
    {
        const Dia::Graphics3D::Mesh3DDrawCommand& cmd = draws[i];

        // Translation-only world placement (SD-MVD-002)
        const Dia::Maths::Vector3D worldPos = cmd.transform.GetTranslation();

        // Determine bounds and colour from asset load state
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

        // Build 8 AABB corners in world space.
        // Corner index = bit pattern (xBit | yBit<<1 | zBit<<2):
        //   bit0=x (0=min, 1=max), bit1=y, bit2=z
        const float xMin = worldPos.X() + localMin.X();
        const float yMin = worldPos.Y() + localMin.Y();
        const float zMin = worldPos.Z() + localMin.Z();
        const float xMax = worldPos.X() + localMax.X();
        const float yMax = worldPos.Y() + localMax.Y();
        const float zMax = worldPos.Z() + localMax.Z();

        // c[0..7]: each corner as Vector3D
        const Dia::Maths::Vector3D c0(xMin, yMin, zMin); // 000
        const Dia::Maths::Vector3D c1(xMax, yMin, zMin); // 100
        const Dia::Maths::Vector3D c2(xMin, yMax, zMin); // 010
        const Dia::Maths::Vector3D c3(xMax, yMax, zMin); // 110
        const Dia::Maths::Vector3D c4(xMin, yMin, zMax); // 001
        const Dia::Maths::Vector3D c5(xMax, yMin, zMax); // 101
        const Dia::Maths::Vector3D c6(xMin, yMax, zMax); // 011
        const Dia::Maths::Vector3D c7(xMax, yMax, zMax); // 111

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
