////////////////////////////////////////////////////////////////////////////////
// Filename: SpriteRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/SpriteRenderer.h"
#include "DiaBgfx/Resources/ShaderProgram.h"
#include "DiaBgfx/Resources/BgfxTextureHandle.h"

#include <DiaGraphics/Frame/EntityFrameData.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace Dia
{
    namespace Bgfx
    {
        // Vertex layout: position (xy), texcoord (uv), colour (rgba packed uint32)
        struct SpriteVertex
        {
            float    x, y;
            float    u, v;
            uint32_t abgr;
        };

        static bgfx::VertexLayout sSpriteLayout;
        static bool               sSpriteLayoutInit = false;

        static void EnsureLayout()
        {
            if (sSpriteLayoutInit)
                return;
            sSpriteLayout.begin()
                .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
                .end();
            sSpriteLayoutInit = true;
        }

        static bgfx::UniformHandle sTexSampler = BGFX_INVALID_HANDLE;

        SpriteRenderer::SpriteRenderer(unsigned short viewId, ShaderProgram* spriteProgram)
            : mViewId(viewId)
            , mSpriteProgram(spriteProgram)
            , mCanvasSize(1.0f, 1.0f)
        {
            EnsureLayout();
            if (!bgfx::isValid(sTexSampler))
                sTexSampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
        }

        SpriteRenderer::~SpriteRenderer()
        {
            // Uniform is shared; cleaned up with bgfx::shutdown
        }

        void SpriteRenderer::OnCanvasSizeChanged(const Dia::Maths::Vector2D& size)
        {
            mCanvasSize = size;
        }

        void SpriteRenderer::Draw(const Dia::Graphics::EntityFrameData& sprites)
        {
            const auto& cmds = sprites.GetSprites();
            if (cmds.Size() == 0)
                return;

            if (!mSpriteProgram || !mSpriteProgram->IsValid())
                return;

            const uint16_t w = static_cast<uint16_t>(mCanvasSize.X());
            const uint16_t h = static_cast<uint16_t>(mCanvasSize.Y());

            // Ortho projection: (0,0) top-left, (w,h) bottom-right, NDC flip
            float ortho[16];
            bx::mtxOrtho(ortho, 0.0f, static_cast<float>(w),
                         static_cast<float>(h), 0.0f,
                         0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
            bgfx::setViewTransform(mViewId, nullptr, ortho);
            bgfx::setViewRect(mViewId, 0, 0, w, h);

            // One draw call per unique ITexture* (batching by texture)
            for (unsigned int i = 0; i < cmds.Size(); ++i)
            {
                const Dia::Graphics::SpriteDrawCommand& cmd = cmds[i];
                if (!cmd.texture || !cmd.texture->IsReady())
                    continue;

                const BgfxTextureHandle* bgfxTex =
                    static_cast<const BgfxTextureHandle*>(cmd.texture);

                const Dia::Maths::Vector2D texSize = cmd.texture->GetSize();
                const float tw = texSize.X();
                const float th = texSize.Y();

                // Determine source rect UV
                float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
                if (tw > 0.0f && th > 0.0f)
                {
                    const Dia::Maths::Vector2D& bl = cmd.textureRect.GetBottomLeft();
                    const Dia::Maths::Vector2D& tr = cmd.textureRect.GetTopRight();
                    float rw = tr.X() - bl.X();
                    float rh = tr.Y() - bl.Y();
                    if (rw > 0.0f && rh > 0.0f)
                    {
                        u0 = bl.X() / tw;
                        v0 = bl.Y() / th;
                        u1 = tr.X() / tw;
                        v1 = tr.Y() / th;
                    }
                }

                // Sprite quad in local space, then transform via cmd fields
                const float hw = (tw * cmd.scale.X()) * 0.5f;
                const float hh = (th * cmd.scale.Y()) * 0.5f;
                const float ox = cmd.origin.X() * cmd.scale.X();
                const float oy = cmd.origin.Y() * cmd.scale.Y();

                const float cosA = bx::cos(bx::toRad(cmd.rotation));
                const float sinA = bx::sin(bx::toRad(cmd.rotation));

                auto rotateAndTranslate = [&](float lx, float ly, float& wx, float& wy)
                {
                    float rx = lx - ox;
                    float ry = ly - oy;
                    wx = cmd.position.X() + rx * cosA - ry * sinA;
                    wy = cmd.position.Y() + rx * sinA + ry * cosA;
                };

                const uint32_t abgr = (static_cast<uint32_t>(cmd.tint.A()) << 24) |
                                      (static_cast<uint32_t>(cmd.tint.B()) << 16) |
                                      (static_cast<uint32_t>(cmd.tint.G()) <<  8) |
                                      (static_cast<uint32_t>(cmd.tint.R()));

                SpriteVertex verts[4];
                rotateAndTranslate(-hw, -hh, verts[0].x, verts[0].y); verts[0].u = u0; verts[0].v = v0; verts[0].abgr = abgr;
                rotateAndTranslate( hw, -hh, verts[1].x, verts[1].y); verts[1].u = u1; verts[1].v = v0; verts[1].abgr = abgr;
                rotateAndTranslate( hw,  hh, verts[2].x, verts[2].y); verts[2].u = u1; verts[2].v = v1; verts[2].abgr = abgr;
                rotateAndTranslate(-hw,  hh, verts[3].x, verts[3].y); verts[3].u = u0; verts[3].v = v1; verts[3].abgr = abgr;

                bgfx::TransientVertexBuffer tvb;
                bgfx::TransientIndexBuffer  tib;
                if (bgfx::getAvailTransientVertexBuffer(4, sSpriteLayout) < 4 ||
                    bgfx::getAvailTransientIndexBuffer(6) < 6)
                {
                    DIA_LOG_WARNING("DiaBgfx", "SpriteRenderer: transient buffer exhausted, skipping sprite");
                    continue;
                }
                bgfx::allocTransientVertexBuffer(&tvb, 4, sSpriteLayout);
                bgfx::allocTransientIndexBuffer(&tib, 6);

                bx::memCopy(tvb.data, verts, sizeof(verts));

                auto* idx = reinterpret_cast<uint16_t*>(tib.data);
                idx[0] = 0; idx[1] = 1; idx[2] = 2;
                idx[3] = 0; idx[4] = 2; idx[5] = 3;

                bgfx::setVertexBuffer(0, &tvb);
                bgfx::setIndexBuffer(&tib);

                const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                                       BGFX_STATE_BLEND_ALPHA;
                bgfx::setState(state);

                bgfx::TextureHandle th2 = { bgfxTex->GetBgfxHandleIdx() };
                bgfx::setTexture(0, sTexSampler, th2);

                bgfx::submit(mViewId,
                             bgfx::ProgramHandle{ mSpriteProgram->GetProgramHandle() });
            }
        }

    } // namespace Bgfx
} // namespace Dia
