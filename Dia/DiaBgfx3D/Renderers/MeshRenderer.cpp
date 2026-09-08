////////////////////////////////////////////////////////////////////////////////
// Filename: MeshRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Renderers/MeshRenderer.h"

#include <bgfx/bgfx.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include "DiaBgfx3D/Resources/MeshGpuCache.h"
#include "DiaBgfx3D/Resources/MaterialRegistry.h"
#include <DiaBgfx/Resources/ShaderProgram.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMaths/Matrix/Matrix44.h>

namespace Dia
{
    namespace Bgfx3D
    {
        MeshRenderer::MeshRenderer(unsigned short viewId, MeshGpuCache* cache,
                                   MaterialRegistry* materials,
                                   Dia::Mesh3D::Mesh3DAssetHandler* meshHandler)
            : mViewId(viewId)
            , mCache(cache)
            , mMaterials(materials)
            , mMeshHandler(meshHandler)
            , mUDirLightDir(bgfx::kInvalidHandle)   // array[kMaxDirLights]
            , mUDirLightColour(bgfx::kInvalidHandle) // array[kMaxDirLights]
            , mUAmbient(bgfx::kInvalidHandle)
            , mUBaseColour(bgfx::kInvalidHandle)
            , mULightViewProj(bgfx::kInvalidHandle)
            , mSShadowMap(bgfx::kInvalidHandle)
            , mSAlbedo(bgfx::kInvalidHandle)
            , mSNormalMap(bgfx::kInvalidHandle)
            , mFlatNormalTexture(0xFFFFu)
            , mWhiteTexture(0xFFFFu)
            , mUCameraPos(bgfx::kInvalidHandle)
            , mUPbrParams(bgfx::kInvalidHandle)
            , mSOrm(bgfx::kInvalidHandle)
            , mDefaultOrmTexture(0xFFFFu)
        {
        }

        MeshRenderer::~MeshRenderer()
        {
            auto destroy = [](unsigned short idx) {
                if (idx == bgfx::kInvalidHandle)
                    return;
                bgfx::UniformHandle h;
                h.idx = idx;
                bgfx::destroy(h);
            };
            destroy(mUDirLightDir);
            destroy(mUDirLightColour);
            destroy(mUAmbient);
            destroy(mUBaseColour);
            destroy(mULightViewProj);
            destroy(mSShadowMap);
            destroy(mSAlbedo);
            destroy(mSNormalMap);
            destroy(mUCameraPos);
            destroy(mUPbrParams);
            destroy(mSOrm);

            if (mDefaultOrmTexture != 0xFFFFu)
            {
                bgfx::TextureHandle h;
                h.idx = mDefaultOrmTexture;
                bgfx::destroy(h);
            }

            if (mFlatNormalTexture != 0xFFFFu)
            {
                bgfx::TextureHandle h;
                h.idx = mFlatNormalTexture;
                bgfx::destroy(h);
            }

            if (mWhiteTexture != 0xFFFFu)
            {
                bgfx::TextureHandle h;
                h.idx = mWhiteTexture;
                bgfx::destroy(h);
            }
        }

        void MeshRenderer::InitUniforms()
        {
            mUDirLightDir    = bgfx::createUniform("u_dirLightDir",    bgfx::UniformType::Vec4, MeshPassLighting::kMaxDirLights).idx;
            mUDirLightColour = bgfx::createUniform("u_dirLightColour", bgfx::UniformType::Vec4, MeshPassLighting::kMaxDirLights).idx;
            mUAmbient        = bgfx::createUniform("u_ambient",                bgfx::UniformType::Vec4).idx;
            mUBaseColour     = bgfx::createUniform("u_baseColour",             bgfx::UniformType::Vec4).idx;
            mULightViewProj  = bgfx::createUniform("u_lightViewProj",         bgfx::UniformType::Mat4).idx;
            mSShadowMap      = bgfx::createUniform("s_shadowMap",             bgfx::UniformType::Sampler).idx;
            mSAlbedo   = bgfx::createUniform("s_albedo",    bgfx::UniformType::Sampler).idx;
            mSNormalMap = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler).idx;
            mUCameraPos  = bgfx::createUniform("u_cameraPos",  bgfx::UniformType::Vec4).idx;
            mUPbrParams  = bgfx::createUniform("u_pbrParams",  bgfx::UniformType::Vec4).idx;
            mSOrm        = bgfx::createUniform("s_orm",        bgfx::UniformType::Sampler).idx;

            // 1×1 default ORM: G=128 (roughness≈0.5), B=0 (metallic=0), R=0 (occlusion unused)
            const uint8_t defaultOrmPixels[4] = { 0, 128, 0, 255 };
            bgfx::TextureHandle defaultOrmTex = bgfx::createTexture2D(1, 1, false, 1,
                bgfx::TextureFormat::RGBA8, 0,
                bgfx::copy(defaultOrmPixels, sizeof(defaultOrmPixels)));
            mDefaultOrmTexture = bgfx::isValid(defaultOrmTex) ? defaultOrmTex.idx : 0xFFFFu;

            // 1×1 flat-normal default: tangent-space "no perturbation" = (128,128,255,255) RGBA8
            const uint8_t flatNormalPixels[4] = { 128, 128, 255, 255 };
            bgfx::TextureHandle flatNormalTex = bgfx::createTexture2D(1, 1, false, 1,
                bgfx::TextureFormat::RGBA8, 0,
                bgfx::copy(flatNormalPixels, sizeof(flatNormalPixels)));
            mFlatNormalTexture = bgfx::isValid(flatNormalTex) ? flatNormalTex.idx : 0xFFFFu;

            // 1×1 white default albedo: no tint = pure white RGBA8
            const uint8_t whitePixels[4] = { 255, 255, 255, 255 };
            bgfx::TextureHandle whiteTex = bgfx::createTexture2D(1, 1, false, 1,
                bgfx::TextureFormat::RGBA8, 0,
                bgfx::copy(whitePixels, sizeof(whitePixels)));
            mWhiteTexture = bgfx::isValid(whiteTex) ? whiteTex.idx : 0xFFFFu;
        }

        void MeshRenderer::Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                const MeshPassLighting& lighting)
        {
            DIA_TRACE_ZONE("mesh_renderer.draw", ::Dia::Observation::Trace::Category::kDiaGraphics);
            const auto& draws = frameData.GetMeshDraws();
            for (unsigned int i = 0; i < draws.Size(); ++i)
            {
                const auto& cmd = draws[i];
                if (cmd.skinningPaletteIndex != 0)
                    continue; // Skip skinned draws
                DrawCommand(cmd, lighting);
            }
        }

        void MeshRenderer::DrawCommand(const Dia::Graphics3D::Mesh3DDrawCommand& cmd,
                                       const MeshPassLighting& lighting)
        {
            // Look up the mesh asset
            Dia::Mesh3D::Mesh3DAsset* asset = mMeshHandler->LookupMesh(cmd.meshId);
            if (!asset)
            {
                bool alreadyWarned = false;
                for (unsigned int i = 0; i < mWarnedMissingCount; ++i)
                {
                    if (mWarnedMissing[i] == cmd.meshId) { alreadyWarned = true; break; }
                }
                if (!alreadyWarned)
                {
                    DIA_LOG_WARNING("DiaBgfx3D", "MeshRenderer: LookupMesh returned null for id=0x%08X (%s)", cmd.meshId.Value(), cmd.meshId.AsChar());
                    if (mWarnedMissingCount < kMaxWarnedIds)
                        mWarnedMissing[mWarnedMissingCount++] = cmd.meshId;
                }
                return;
            }
            if (!asset->IsReady())
            {
                DIA_LOG_WARNING("DiaBgfx3D", "MeshRenderer: asset 0x%08X not ready (state=%d)", cmd.meshId.Value(), static_cast<int>(asset->GetState()));
                return;
            }

            // Get GPU buffers
            const GpuMesh* gpu = mCache->GetOrUpload(*asset);
            if (!gpu)
                return;

            // Model transform — set once; shared by all submesh draws below.
            float mtx[16];
            cmd.transform.GetColumnMajor(mtx);
            bgfx::setTransform(mtx);

            // --- Uniforms shared across all submeshes ---
            {
                bgfx::UniformHandle h;

                h.idx = mUDirLightDir;
                bgfx::setUniform(h, lighting.dirLightDir, MeshPassLighting::kMaxDirLights);

                h.idx = mUDirLightColour;
                bgfx::setUniform(h, lighting.dirLightColour, MeshPassLighting::kMaxDirLights);

                h.idx = mUAmbient;
                bgfx::setUniform(h, lighting.ambient);

                h.idx = mULightViewProj;
                bgfx::setUniform(h, lighting.lightViewProj);

                h.idx = mUCameraPos;
                bgfx::setUniform(h, lighting.cameraPos);
            }

            // --- Shadow map sampler ---
            if (lighting.shadowTexture != bgfx::kInvalidHandle)
            {
                bgfx::UniformHandle samplerHandle;
                samplerHandle.idx = mSShadowMap;
                bgfx::TextureHandle shadowTex;
                shadowTex.idx = lighting.shadowTexture;
                bgfx::setTexture(2, samplerHandle, shadowTex);
            }

            // TODO: verify winding order matches glTF (CCW) vs procedural meshes (CW).
            // Culling disabled temporarily to diagnose invisible avocado.
            const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                                 | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;

            bgfx::VertexBufferHandle vbh{ gpu->vertexBuffer };
            bgfx::IndexBufferHandle  ibh{ gpu->indexBuffer };

            // --- Draw each submesh with its own material ---
            const auto& submeshes = asset->GetSubmeshes();
            for (unsigned int si = 0; si < submeshes.Size(); ++si)
            {
                const Dia::Mesh3D::Submesh& sub = submeshes[si];

                // Resolve submesh material; fall back to command materialId, then default.
                const MaterialDescriptor* mat = mMaterials->Resolve(sub.materialId);
                if (!mat)
                    mat = mMaterials->Resolve(cmd.materialId);
                if (!mat)
                    mat = &mMaterials->GetDefault();

                if (!mat->program || !mat->program->IsValid())
                {
                    DIA_LOG_WARNING("DiaBgfx3D", "MeshRenderer: submesh %u skipped — material has no valid program (matId=0x%08X)", si, mat->id.Value());
                    continue;
                }
                bgfx::ProgramHandle prog{ mat->program->GetProgramHandle() };

                // Unpack baseColourRGBA (0xRRGGBBAA) to float4 [0,1]
                float baseColour[4];
                baseColour[0] = static_cast<float>((mat->baseColourRGBA >> 24) & 0xFF) / 255.0f;
                baseColour[1] = static_cast<float>((mat->baseColourRGBA >> 16) & 0xFF) / 255.0f;
                baseColour[2] = static_cast<float>((mat->baseColourRGBA >>  8) & 0xFF) / 255.0f;
                baseColour[3] = static_cast<float>((mat->baseColourRGBA      ) & 0xFF) / 255.0f;
                bgfx::UniformHandle hBase;
                hBase.idx = mUBaseColour;
                bgfx::setUniform(hBase, baseColour);

                // --- Albedo texture (slot 0) ---
                {
                    bgfx::UniformHandle sAlbedo;
                    sAlbedo.idx = mSAlbedo;
                    bgfx::TextureHandle albedoTex;
                    albedoTex.idx = (mat->albedoTexture != 0xFFFFu) ? mat->albedoTexture : mWhiteTexture;
                    bgfx::setTexture(0, sAlbedo, albedoTex);
                }

                // --- Normal map texture (slot 1) ---
                {
                    bgfx::UniformHandle sNormalMap;
                    sNormalMap.idx = mSNormalMap;
                    bgfx::TextureHandle normalTex;
                    normalTex.idx = (mat->normalMapTexture != 0xFFFFu) ? mat->normalMapTexture : mFlatNormalTexture;
                    bgfx::setTexture(1, sNormalMap, normalTex);
                }

                // --- ORM texture (slot 3) + PBR params ---
                {
                    bgfx::UniformHandle sOrm;
                    sOrm.idx = mSOrm;
                    bgfx::TextureHandle ormTex;
                    ormTex.idx = (mat->ormTexture != 0xFFFFu) ? mat->ormTexture : mDefaultOrmTexture;
                    bgfx::setTexture(3, sOrm, ormTex);

                    float pbrParams[4] = { mat->metallic, mat->roughness, 0.0f, 0.0f };
                    bgfx::UniformHandle hPbr;
                    hPbr.idx = mUPbrParams;
                    bgfx::setUniform(hPbr, pbrParams);
                }

                bgfx::setVertexBuffer(0, vbh);
                bgfx::setIndexBuffer(ibh, sub.indexStart, sub.indexCount);
                bgfx::setState(state);
                bgfx::submit(mViewId, prog);
            }
        }

    } // namespace Bgfx3D
} // namespace Dia
