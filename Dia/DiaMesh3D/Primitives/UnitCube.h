////////////////////////////////////////////////////////////////////////////////
// Filename: UnitCube.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector4D.h>

namespace Dia { namespace Mesh3D { namespace Primitives {

// Returns a heap-allocated Mesh3DAsset representing a unit cube (caller owns it,
// or pass to Mesh3DAssetHandler::RegisterMesh to transfer ownership).
// 24 vertices (4 per face), 36 indices (2 triangles per face), 1 submesh.
// Cube spans [-0.5, +0.5] on all axes.
inline Mesh3DAsset* CreateUnitCube(Dia::Core::StringCRC assetId)
{
    // clang-format off
    // Face normals
    const Dia::Maths::Vector3D nPX( 1.0f,  0.0f,  0.0f);
    const Dia::Maths::Vector3D nNX(-1.0f,  0.0f,  0.0f);
    const Dia::Maths::Vector3D nPY( 0.0f,  1.0f,  0.0f);
    const Dia::Maths::Vector3D nNY( 0.0f, -1.0f,  0.0f);
    const Dia::Maths::Vector3D nPZ( 0.0f,  0.0f,  1.0f);
    const Dia::Maths::Vector3D nNZ( 0.0f,  0.0f, -1.0f);
    // Face tangents (x-axis of each face's UV space); w=bitangent sign
    const Dia::Maths::Vector4D tPX( 0.0f,  0.0f, -1.0f, 1.0f);
    const Dia::Maths::Vector4D tNX( 0.0f,  0.0f,  1.0f, 1.0f);
    const Dia::Maths::Vector4D tPY( 1.0f,  0.0f,  0.0f, 1.0f);
    const Dia::Maths::Vector4D tNY(-1.0f,  0.0f,  0.0f, 1.0f);
    const Dia::Maths::Vector4D tPZ( 1.0f,  0.0f,  0.0f, 1.0f);
    const Dia::Maths::Vector4D tNZ(-1.0f,  0.0f,  0.0f, 1.0f);
    const uint32_t white = 0xFFFFFFFFu;

    Vertex3D verts[24];

    // +X face
    verts[ 0].position = Dia::Maths::Vector3D( 0.5f, -0.5f, -0.5f); verts[ 0].normal = nPX; verts[ 0].tangent = tPX; verts[ 0].uv0 = Dia::Maths::Vector2D(0.0f, 1.0f); verts[ 0].colour = white;
    verts[ 1].position = Dia::Maths::Vector3D( 0.5f, -0.5f,  0.5f); verts[ 1].normal = nPX; verts[ 1].tangent = tPX; verts[ 1].uv0 = Dia::Maths::Vector2D(1.0f, 1.0f); verts[ 1].colour = white;
    verts[ 2].position = Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f); verts[ 2].normal = nPX; verts[ 2].tangent = tPX; verts[ 2].uv0 = Dia::Maths::Vector2D(1.0f, 0.0f); verts[ 2].colour = white;
    verts[ 3].position = Dia::Maths::Vector3D( 0.5f,  0.5f, -0.5f); verts[ 3].normal = nPX; verts[ 3].tangent = tPX; verts[ 3].uv0 = Dia::Maths::Vector2D(0.0f, 0.0f); verts[ 3].colour = white;

    // -X face
    verts[ 4].position = Dia::Maths::Vector3D(-0.5f, -0.5f,  0.5f); verts[ 4].normal = nNX; verts[ 4].tangent = tNX; verts[ 4].uv0 = Dia::Maths::Vector2D(0.0f, 1.0f); verts[ 4].colour = white;
    verts[ 5].position = Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f); verts[ 5].normal = nNX; verts[ 5].tangent = tNX; verts[ 5].uv0 = Dia::Maths::Vector2D(1.0f, 1.0f); verts[ 5].colour = white;
    verts[ 6].position = Dia::Maths::Vector3D(-0.5f,  0.5f, -0.5f); verts[ 6].normal = nNX; verts[ 6].tangent = tNX; verts[ 6].uv0 = Dia::Maths::Vector2D(1.0f, 0.0f); verts[ 6].colour = white;
    verts[ 7].position = Dia::Maths::Vector3D(-0.5f,  0.5f,  0.5f); verts[ 7].normal = nNX; verts[ 7].tangent = tNX; verts[ 7].uv0 = Dia::Maths::Vector2D(0.0f, 0.0f); verts[ 7].colour = white;

    // +Y face
    verts[ 8].position = Dia::Maths::Vector3D(-0.5f,  0.5f, -0.5f); verts[ 8].normal = nPY; verts[ 8].tangent = tPY; verts[ 8].uv0 = Dia::Maths::Vector2D(0.0f, 1.0f); verts[ 8].colour = white;
    verts[ 9].position = Dia::Maths::Vector3D( 0.5f,  0.5f, -0.5f); verts[ 9].normal = nPY; verts[ 9].tangent = tPY; verts[ 9].uv0 = Dia::Maths::Vector2D(1.0f, 1.0f); verts[ 9].colour = white;
    verts[10].position = Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f); verts[10].normal = nPY; verts[10].tangent = tPY; verts[10].uv0 = Dia::Maths::Vector2D(1.0f, 0.0f); verts[10].colour = white;
    verts[11].position = Dia::Maths::Vector3D(-0.5f,  0.5f,  0.5f); verts[11].normal = nPY; verts[11].tangent = tPY; verts[11].uv0 = Dia::Maths::Vector2D(0.0f, 0.0f); verts[11].colour = white;

    // -Y face
    verts[12].position = Dia::Maths::Vector3D(-0.5f, -0.5f,  0.5f); verts[12].normal = nNY; verts[12].tangent = tNY; verts[12].uv0 = Dia::Maths::Vector2D(0.0f, 1.0f); verts[12].colour = white;
    verts[13].position = Dia::Maths::Vector3D( 0.5f, -0.5f,  0.5f); verts[13].normal = nNY; verts[13].tangent = tNY; verts[13].uv0 = Dia::Maths::Vector2D(1.0f, 1.0f); verts[13].colour = white;
    verts[14].position = Dia::Maths::Vector3D( 0.5f, -0.5f, -0.5f); verts[14].normal = nNY; verts[14].tangent = tNY; verts[14].uv0 = Dia::Maths::Vector2D(1.0f, 0.0f); verts[14].colour = white;
    verts[15].position = Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f); verts[15].normal = nNY; verts[15].tangent = tNY; verts[15].uv0 = Dia::Maths::Vector2D(0.0f, 0.0f); verts[15].colour = white;

    // +Z face
    verts[16].position = Dia::Maths::Vector3D( 0.5f, -0.5f,  0.5f); verts[16].normal = nPZ; verts[16].tangent = tPZ; verts[16].uv0 = Dia::Maths::Vector2D(0.0f, 1.0f); verts[16].colour = white;
    verts[17].position = Dia::Maths::Vector3D(-0.5f, -0.5f,  0.5f); verts[17].normal = nPZ; verts[17].tangent = tPZ; verts[17].uv0 = Dia::Maths::Vector2D(1.0f, 1.0f); verts[17].colour = white;
    verts[18].position = Dia::Maths::Vector3D(-0.5f,  0.5f,  0.5f); verts[18].normal = nPZ; verts[18].tangent = tPZ; verts[18].uv0 = Dia::Maths::Vector2D(1.0f, 0.0f); verts[18].colour = white;
    verts[19].position = Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f); verts[19].normal = nPZ; verts[19].tangent = tPZ; verts[19].uv0 = Dia::Maths::Vector2D(0.0f, 0.0f); verts[19].colour = white;

    // -Z face
    verts[20].position = Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f); verts[20].normal = nNZ; verts[20].tangent = tNZ; verts[20].uv0 = Dia::Maths::Vector2D(0.0f, 1.0f); verts[20].colour = white;
    verts[21].position = Dia::Maths::Vector3D( 0.5f, -0.5f, -0.5f); verts[21].normal = nNZ; verts[21].tangent = tNZ; verts[21].uv0 = Dia::Maths::Vector2D(1.0f, 1.0f); verts[21].colour = white;
    verts[22].position = Dia::Maths::Vector3D( 0.5f,  0.5f, -0.5f); verts[22].normal = nNZ; verts[22].tangent = tNZ; verts[22].uv0 = Dia::Maths::Vector2D(1.0f, 0.0f); verts[22].colour = white;
    verts[23].position = Dia::Maths::Vector3D(-0.5f,  0.5f, -0.5f); verts[23].normal = nNZ; verts[23].tangent = tNZ; verts[23].uv0 = Dia::Maths::Vector2D(0.0f, 0.0f); verts[23].colour = white;

    // 2 triangles per face (counter-clockwise winding when viewed from outside)
    uint16_t indices[36];
    for (uint16_t f = 0; f < 6; ++f)
    {
        uint16_t b = static_cast<uint16_t>(f * 4);
        indices[f * 6 + 0] = b + 0;
        indices[f * 6 + 1] = b + 1;
        indices[f * 6 + 2] = b + 2;
        indices[f * 6 + 3] = b + 0;
        indices[f * 6 + 4] = b + 2;
        indices[f * 6 + 5] = b + 3;
    }

    Submesh sub;
    sub.indexStart = 0;
    sub.indexCount = 36;
    sub.materialId = Dia::Core::StringCRC("default_3d");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f),
        Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f));
    // clang-format on

    Mesh3DAsset* asset = new Mesh3DAsset(assetId);
    asset->Populate(verts, 24, indices, 36, &sub, 1, bounds);
    return asset;
}

} } }
