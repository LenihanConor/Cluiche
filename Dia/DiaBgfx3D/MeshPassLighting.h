////////////////////////////////////////////////////////////////////////////////
// Filename: MeshPassLighting.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia { namespace Bgfx3D {

// Per-frame lighting inputs assembled by Canvas3D and forwarded to MeshRenderer.
// Carries the combined output of ShadowRenderer (lightViewProj, shadowTexture)
// and the directional/ambient light extracted from Mesh3DFrameData.
struct MeshPassLighting
{
    float          dirLightDir[4];    // xyz = normalised direction (toward light), w = 0
    float          dirLightColour[4]; // rgb = colour, a = intensity
    float          ambient[4];        // rgb = colour, a = intensity
    float          lightViewProj[16]; // column-major, for u_lightViewProj in vs_mesh
    unsigned short shadowTexture;     // bgfx::TextureHandle::idx (shadow depth)
    float          cameraPos[4];      // xyz = camera world position, w = 0
};

} } // namespace Dia::Bgfx3D
