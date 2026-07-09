////////////////////////////////////////////////////////////////////////////////
// Filename: MeshRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; struct Mesh3DDrawCommand; } }
namespace Dia { namespace Mesh3D { class Mesh3DAssetHandler; } }

#include <DiaBgfx3D/MeshPassLighting.h>

namespace Dia
{
    namespace Bgfx3D
    {
        class MeshGpuCache;
        class MaterialRegistry;

        class MeshRenderer
        {
        public:
            MeshRenderer(unsigned short viewId, MeshGpuCache* cache,
                         MaterialRegistry* materials,
                         Dia::Mesh3D::Mesh3DAssetHandler* meshHandler);
            ~MeshRenderer();

            // Must be called once after bgfx::init() before any Draw(). Separated
            // from the ctor so construction is bgfx-free (unit-test friendly).
            void InitUniforms();

            void Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                      const MeshPassLighting& lighting);

        private:
            void DrawCommand(const Dia::Graphics3D::Mesh3DDrawCommand& cmd,
                             const MeshPassLighting& lighting);

            unsigned short                   mViewId;
            MeshGpuCache*                    mCache;
            MaterialRegistry*                mMaterials;
            Dia::Mesh3D::Mesh3DAssetHandler* mMeshHandler;

            // bgfx uniform handles — created once in ctor, destroyed in dtor.
            unsigned short  mUDirLightDir;     // bgfx::UniformHandle::idx
            unsigned short  mUDirLightColour;
            unsigned short  mUAmbient;
            unsigned short  mUBaseColour;
            unsigned short  mULightViewProj;
            unsigned short  mSShadowMap;       // bgfx::UniformHandle::idx (sampler)
            unsigned short  mSAlbedo;          // bgfx::UniformHandle::idx (sampler)
            unsigned short  mSNormalMap;       // bgfx::UniformHandle::idx (sampler)
            unsigned short  mFlatNormalTexture; // bgfx::TextureHandle::idx (1×1 flat-normal default)
            unsigned short  mWhiteTexture;      // bgfx::TextureHandle::idx (1×1 white albedo default)
            unsigned short  mUCameraPos;        // bgfx::UniformHandle::idx — u_cameraPos vec4
            unsigned short  mUPbrParams;        // bgfx::UniformHandle::idx — u_pbrParams vec4
            unsigned short  mSOrm;              // bgfx::UniformHandle::idx (sampler) — s_orm
            unsigned short  mDefaultOrmTexture; // bgfx::TextureHandle::idx — 1×1 ORM default
        };

    } // namespace Bgfx3D
} // namespace Dia
