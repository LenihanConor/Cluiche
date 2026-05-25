////////////////////////////////////////////////////////////////////////////////
// Filename: UIOverlayRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/UIOverlayRenderer.h"
#include "DiaBgfx/Resources/ShaderProgram.h"

#include <DiaUI/UIDataBuffer.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace Dia
{
    namespace Bgfx
    {
        // Fullscreen quad vertex: position in NDC [-1,1] + texcoord
        struct UIVertex
        {
            float x, y;
            float u, v;
        };

        static bgfx::VertexLayout sUILayout;
        static bool               sUILayoutInit = false;

        static bgfx::UniformHandle sUITexSampler = BGFX_INVALID_HANDLE;

        static void EnsureUILayout()
        {
            if (sUILayoutInit)
                return;
            sUILayout.begin()
                .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .end();
            sUILayoutInit = true;
            if (!bgfx::isValid(sUITexSampler))
                sUITexSampler = bgfx::createUniform("s_uiOverlay", bgfx::UniformType::Sampler);
        }

        UIOverlayRenderer::UIOverlayRenderer(unsigned short viewId, ShaderProgram* uiProgram)
            : mViewId(viewId)
            , mUiProgram(uiProgram)
            , mTextureHandle(bgfx::kInvalidHandle)
            , mCanvasSize(1.0f, 1.0f)
        {
            EnsureUILayout();
        }

        UIOverlayRenderer::~UIOverlayRenderer()
        {
            if (mTextureHandle != bgfx::kInvalidHandle)
            {
                bgfx::destroy(bgfx::TextureHandle{ mTextureHandle });
                mTextureHandle = bgfx::kInvalidHandle;
            }
        }

        void UIOverlayRenderer::OnCanvasSizeChanged(const Dia::Maths::Vector2D& size)
        {
            mCanvasSize = size;

            // Recreate texture at new size
            if (mTextureHandle != bgfx::kInvalidHandle)
            {
                bgfx::destroy(bgfx::TextureHandle{ mTextureHandle });
                mTextureHandle = bgfx::kInvalidHandle;
            }

            const uint16_t w = static_cast<uint16_t>(size.X());
            const uint16_t h = static_cast<uint16_t>(size.Y());
            if (w == 0 || h == 0)
                return;

            bgfx::TextureHandle tex = bgfx::createTexture2D(
                w, h, false, 1, bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, nullptr);

            if (!bgfx::isValid(tex))
            {
                DIA_LOG_ERROR("DiaBgfx", "UIOverlayRenderer: failed to create overlay texture (%ux%u)", w, h);
                return;
            }
            mTextureHandle = tex.idx;
        }

        void UIOverlayRenderer::Composite(const Dia::UI::UIDataBuffer& buffer)
        {
            if (!mUiProgram || !mUiProgram->IsValid())
                return;

            const uint16_t w = static_cast<uint16_t>(mCanvasSize.X());
            const uint16_t h = static_cast<uint16_t>(mCanvasSize.Y());
            if (w == 0 || h == 0)
                return;

            // Lazy-create texture on first Composite if OnCanvasSizeChanged was not called yet
            if (mTextureHandle == bgfx::kInvalidHandle)
            {
                bgfx::TextureHandle tex = bgfx::createTexture2D(
                    w, h, false, 1, bgfx::TextureFormat::RGBA8,
                    BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, nullptr);
                if (!bgfx::isValid(tex))
                {
                    DIA_LOG_ERROR("DiaBgfx", "UIOverlayRenderer: failed to lazy-create overlay texture");
                    return;
                }
                mTextureHandle = tex.idx;
            }

            // Upload fresh pixel data if available
            if (buffer.GetBufferSize() > 0)
            {
                const uint32_t dataSize = static_cast<uint32_t>(buffer.GetBufferSize());
                const bgfx::Memory* mem = bgfx::copy(buffer.GetBuffer(), dataSize);
                bgfx::updateTexture2D(
                    bgfx::TextureHandle{ mTextureHandle },
                    0, 0,   // layer, mip
                    0, 0,   // x, y
                    w, h,
                    mem,
                    w * 4u  // pitch
                );
            }

            // Draw fullscreen quad in view-space
            bgfx::setViewRect(mViewId, 0, 0, w, h);
            bgfx::setViewClear(mViewId, BGFX_CLEAR_NONE);

            // NDC quad (top-left origin after ortho)
            float ortho[16];
            bx::mtxOrtho(ortho, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                         0.0f, bgfx::getCaps()->homogeneousDepth);
            bgfx::setViewTransform(mViewId, nullptr, ortho);

            UIVertex verts[4] = {
                { 0.0f, 0.0f, 0.0f, 0.0f },
                { 1.0f, 0.0f, 1.0f, 0.0f },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                { 0.0f, 1.0f, 0.0f, 1.0f },
            };

            if (bgfx::getAvailTransientVertexBuffer(4, sUILayout) < 4 ||
                bgfx::getAvailTransientIndexBuffer(6) < 6)
                return;

            bgfx::TransientVertexBuffer tvb;
            bgfx::TransientIndexBuffer  tib;
            bgfx::allocTransientVertexBuffer(&tvb, 4, sUILayout);
            bgfx::allocTransientIndexBuffer(&tib, 6);

            bx::memCopy(tvb.data, verts, sizeof(verts));
            auto* idx = reinterpret_cast<uint16_t*>(tib.data);
            idx[0] = 0; idx[1] = 1; idx[2] = 2;
            idx[3] = 0; idx[4] = 2; idx[5] = 3;

            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            bgfx::setTexture(0, sUITexSampler, bgfx::TextureHandle{ mTextureHandle });
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_ALPHA);

            bgfx::submit(mViewId, bgfx::ProgramHandle{ mUiProgram->GetProgramHandle() });
        }

    } // namespace Bgfx
} // namespace Dia
