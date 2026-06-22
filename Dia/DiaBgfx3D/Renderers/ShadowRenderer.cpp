////////////////////////////////////////////////////////////////////////////////
// Filename: ShadowRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Renderers/ShadowRenderer.h"
#include "DiaBgfx3D/Resources/MeshGpuCache.h"
#include <DiaBgfx/Resources/ShaderProgram.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics3D/Light.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>

#include <bgfx/bgfx.h>

namespace Dia { namespace Bgfx3D {

static constexpr uint16_t kShadowMapSize = 2048;

ShadowRenderer::ShadowRenderer(unsigned short shadowViewId, MeshGpuCache* cache)
    : mViewId(shadowViewId)
    , mShadowFramebuffer(bgfx::kInvalidHandle)
    , mShadowTexture(bgfx::kInvalidHandle)
    , mCache(cache)
    , mShadowProgram(nullptr)
    , mLightViewProj{}
{
    // Identity matrix as safe default (column-major)
    mLightViewProj[0]  = 1.0f; mLightViewProj[5]  = 1.0f;
    mLightViewProj[10] = 1.0f; mLightViewProj[15] = 1.0f;
    // Create 2048x2048 depth texture
    bgfx::TextureHandle depthTex = bgfx::createTexture2D(
        kShadowMapSize,
        kShadowMapSize,
        false,
        1,
        bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT
    );
    mShadowTexture = depthTex.idx;

    // Create framebuffer from depth texture
    bgfx::FrameBufferHandle fbHandle = bgfx::createFrameBuffer(1, &depthTex, false);
    mShadowFramebuffer = fbHandle.idx;

    // Configure shadow view
    bgfx::setViewClear(mViewId, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
    bgfx::setViewRect(mViewId, 0, 0, kShadowMapSize, kShadowMapSize);
    bgfx::setViewFrameBuffer(mViewId, fbHandle);
}

ShadowRenderer::~ShadowRenderer()
{
    // Destroy framebuffer first (refs the texture)
    if (mShadowFramebuffer != bgfx::kInvalidHandle)
    {
        bgfx::FrameBufferHandle fbHandle;
        fbHandle.idx = mShadowFramebuffer;
        bgfx::destroy(fbHandle);
    }

    // Then destroy depth texture
    if (mShadowTexture != bgfx::kInvalidHandle)
    {
        bgfx::TextureHandle texHandle;
        texHandle.idx = mShadowTexture;
        bgfx::destroy(texHandle);
    }
}

void ShadowRenderer::RenderShadowMap(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                      Dia::Mesh3D::Mesh3DAssetHandler* meshHandler)
{
    DIA_TRACE_ZONE("shadow_renderer.render_shadow_map", ::Dia::Observation::Trace::Category::kDiaGraphics);
    DIA_PROFILE_SCOPE("shadow_renderer.render_shadow_map", ::Dia::Observation::Profile::Category::kDiaGraphics);
    const auto& dirLights = frameData.GetDirectionalLights();
    if (dirLights.Size() == 0)
    {
        return; // No directional light, no shadow source
    }

    // The shadow program is invariant across the whole pass — validate it once.
    // The invalid sentinel is bgfx::kInvalidHandle (0xffff), NOT 0; handle 0 is a
    // real handle (the first program created), so a 0 fallback would render the
    // shadow pass with the wrong shader. If there is no valid program, skip the
    // entire pass rather than submit garbage.
    if (mShadowProgram == nullptr || !mShadowProgram->IsValid())
    {
        return;
    }
    const bgfx::ProgramHandle shadowProgram{ mShadowProgram->GetProgramHandle() };

    // Build light-space view-projection for first directional light
    const Dia::Graphics3D::DirectionalLight& light = dirLights[0];
    Dia::Maths::Vector3D lightDir = light.direction;
    Dia::Maths::Vector3D lightPos = lightDir * -100.0f; // back along light dir

    Dia::Graphics3D::Camera3D lightCam;
    lightCam.SetView(lightPos, Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f), Dia::Maths::Vector3D(0.0f, 1.0f, 0.0f));
    lightCam.SetOrthographic(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 200.0f);

    float viewMtx[16], projMtx[16];
    lightCam.view.GetColumnMajor(viewMtx);
    lightCam.projection.GetColumnMajor(projMtx);
    bgfx::setViewTransform(mViewId, viewMtx, projMtx);

    // Cache view*proj for the mesh pass to compute shadow coords in the vertex shader uniform.
    // bgfx requires column-major; store combined as proj * view (applied right-to-left).
    Dia::Maths::Matrix44 lightVP = lightCam.projection * lightCam.view;
    lightVP.GetColumnMajor(mLightViewProj);

    // Render all visible meshes from light POV
    const auto& meshDraws = frameData.GetMeshDraws();
    for (uint32_t i = 0; i < meshDraws.Size(); ++i)
    {
        const Dia::Graphics3D::Mesh3DDrawCommand& cmd = meshDraws[i];

        // Lookup asset
        Dia::Mesh3D::Mesh3DAsset* asset = meshHandler->LookupMesh(cmd.meshId);
        if (asset == nullptr || !asset->IsReady())
        {
            continue;
        }

        // Get GPU buffers
        const GpuMesh* gpuMesh = mCache->GetOrUpload(*asset);
        if (gpuMesh == nullptr)
        {
            continue;
        }

        // Set model transform
        float modelMtx[16];
        cmd.transform.GetColumnMajor(modelMtx);
        bgfx::setTransform(modelMtx);

        // Set depth-only state: write depth, test depth, cull CW
        bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW);

        // Set vertex/index buffers
        bgfx::VertexBufferHandle vbh;
        vbh.idx = gpuMesh->vertexBuffer;
        bgfx::IndexBufferHandle ibh;
        ibh.idx = gpuMesh->indexBuffer;
        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);

        // Submit with the shadow program validated once at the top of the pass.
        bgfx::submit(mViewId, shadowProgram);
    }
}

void ShadowRenderer::SetProgram(Dia::Bgfx::ShaderProgram* prog)
{
    mShadowProgram = prog;
}

unsigned short ShadowRenderer::GetShadowFramebuffer() const
{
    return mShadowFramebuffer;
}

unsigned short ShadowRenderer::GetShadowTexture() const
{
    return mShadowTexture;
}

unsigned short ShadowRenderer::GetShadowViewId() const
{
    return mViewId;
}

void ShadowRenderer::GetLightViewProj(float outMtx16[16]) const
{
    for (int i = 0; i < 16; ++i)
        outMtx16[i] = mLightViewProj[i];
}

} } // namespace Dia::Bgfx3D
