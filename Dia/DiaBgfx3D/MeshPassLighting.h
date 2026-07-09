////////////////////////////////////////////////////////////////////////////////
// Filename: MeshPassLighting.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia { namespace Bgfx3D {

// Per-frame lighting inputs assembled by Canvas3D and forwarded to MeshRenderer.
// Carries the combined output of ShadowRenderer (lightViewProj, shadowTexture)
// and all directional lights + ambient extracted from Mesh3DFrameData.
struct MeshPassLighting
{
    static constexpr int kMaxDirLights = 8;

    float          dirLightDir[4 * kMaxDirLights];    // kMaxDirLights × vec4: xyz = dir, w = 0; zero-padded
    float          dirLightColour[4 * kMaxDirLights]; // kMaxDirLights × vec4: rgb = colour, a = intensity
    float          ambient[4];                        // rgb = colour, a = intensity
    float          lightViewProj[16];                 // column-major, for u_lightViewProj in vs_mesh
    unsigned short shadowTexture;                     // bgfx::TextureHandle::idx (shadow depth, dirLights[0] only)
    float          cameraPos[4];                      // xyz = camera world position, w = 0
};

} } // namespace Dia::Bgfx3D
