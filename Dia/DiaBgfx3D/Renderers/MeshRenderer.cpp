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
            , mUDirLightDir(bgfx::kInvalidHandle)
            , mUDirLightColour(bgfx::kInvalidHandle)
            , mUAmbient(bgfx::kInvalidHandle)
            , mUBaseColour(bgfx::kInvalidHandle)
            , mULightViewProj(bgfx::kInvalidHandle)
            , mSShadowMap(bgfx::kInvalidHandle)
            , mFlatNormalTexture(0xFFFFu)
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

            if (mFlatNormalTexture != 0xFFFFu)
            {
                bgfx::TextureHandle h;
                h.idx = mFlatNormalTexture;
                bgfx::destroy(h);
            }
        }

        void MeshRenderer::InitUniforms()
        {
            mUDirLightDir    = bgfx::createUniform("u_directionalLightDir",    bgfx::UniformType::Vec4).idx;
            mUDirLightColour = bgfx::createUniform("u_directionalLightColour", bgfx::UniformType::Vec4).idx;
            mUAmbient        = bgfx::createUniform("u_ambient",                bgfx::UniformType::Vec4).idx;
            mUBaseColour     = bgfx::createUniform("u_baseColour",             bgfx::UniformType::Vec4).idx;
            mULightViewProj  = bgfx::createUniform("u_lightViewProj",         bgfx::UniformType::Mat4).idx;
            mSShadowMap      = bgfx::createUniform("s_shadowMap",             bgfx::UniformType::Sampler).idx;

            // 1×1 flat-normal default: tangent-space "no perturbation" = (128,128,255,255) RGBA8
            const uint8_t flatNormalPixels[4] = { 128, 128, 255, 255 };
            bgfx::TextureHandle flatNormalTex = bgfx::createTexture2D(1, 1, false, 1,
                bgfx::TextureFormat::RGBA8, 0,
                bgfx::copy(flatNormalPixels, sizeof(flatNormalPixels)));
            mFlatNormalTexture = bgfx::isValid(flatNormalTex) ? flatNormalTex.idx : 0xFFFFu;
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
            if (!asset || !asset->IsReady())
                return;

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
                bgfx::setUniform(h, lighting.dirLightDir);

                h.idx = mUDirLightColour;
                bgfx::setUniform(h, lighting.dirLightColour);

                h.idx = mUAmbient;
                bgfx::setUniform(h, lighting.ambient);

                h.idx = mULightViewProj;
                bgfx::setUniform(h, lighting.lightViewProj);
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

            const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                                 | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
                                 | BGFX_STATE_CULL_CW;

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
                    DIA_LOG_DEBUG("DiaBgfx3D", "MeshRenderer: submesh %u skipped — material has no valid program", si);
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

                bgfx::setVertexBuffer(0, vbh);
                bgfx::setIndexBuffer(ibh, sub.indexStart, sub.indexCount);
                bgfx::setState(state);
                bgfx::submit(mViewId, prog);
            }
        }

    } // namespace Bgfx3D
} // namespace Dia
