////////////////////////////////////////////////////////////////////////////////
// Filename: ShadowRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }
namespace Dia { namespace Mesh3D { class Mesh3DAssetHandler; } }

namespace Dia { namespace Bgfx3D {

class MeshGpuCache;

// Renders depth-only shadow map from directional light POV.
// Single-cascade 2048x2048 depth target (BG3-007).
// No bgfx types in public header (BG3-005).
class ShadowRenderer
{
public:
    ShadowRenderer(unsigned short shadowViewId, MeshGpuCache* cache);
    ~ShadowRenderer();

    void RenderShadowMap(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                         Dia::Mesh3D::Mesh3DAssetHandler* meshHandler);

    unsigned short GetShadowFramebuffer() const;
    unsigned short GetShadowViewId() const;

private:
    unsigned short mViewId;
    unsigned short mShadowFramebuffer;  // bgfx::FrameBufferHandle::idx
    unsigned short mShadowTexture;      // bgfx::TextureHandle::idx (depth)
    MeshGpuCache*  mCache;
};

} } // namespace Dia::Bgfx3D
