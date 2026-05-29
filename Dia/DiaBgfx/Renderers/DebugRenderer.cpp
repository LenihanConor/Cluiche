////////////////////////////////////////////////////////////////////////////////
// Filename: DebugRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/DebugRenderer.h"
#include "DiaBgfx/Resources/ShaderProgram.h"

#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <vector>
#include <cmath>

namespace Dia
{
    namespace Bgfx
    {
        // Vertex layout for debug primitives: position + colour
        struct DebugVertex
        {
            float    x, y;
            uint32_t abgr;
        };

        static bgfx::VertexLayout sDebugLayout;
        static bool               sDebugLayoutInit = false;

        static void EnsureDebugLayout()
        {
            if (sDebugLayoutInit)
                return;
            sDebugLayout.begin()
                .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
                .end();
            sDebugLayoutInit = true;
        }

        static uint32_t ToABGR(const Dia::Graphics::RGBA& c)
        {
            return (static_cast<uint32_t>(c.A()) << 24) |
                   (static_cast<uint32_t>(c.B()) << 16) |
                   (static_cast<uint32_t>(c.G()) <<  8) |
                   (static_cast<uint32_t>(c.R()));
        }

        // ---------- batch accumulators (per-frame, stack-allocated via std::vector) ----------

        struct DebugBatch
        {
            std::vector<DebugVertex> lines;     // pairs of verts
            std::vector<DebugVertex> tris;      // triples of verts
            std::vector<DebugVertex> points;    // single verts
        };

        // ---------- helpers ----------

        static void PushLine(DebugBatch& b,
                             float x0, float y0, float x1, float y1,
                             uint32_t abgr)
        {
            b.lines.push_back({ x0, y0, abgr });
            b.lines.push_back({ x1, y1, abgr });
        }

        static void PushTri(DebugBatch& b,
                            float ax, float ay,
                            float bx, float by,
                            float cx, float cy,
                            uint32_t abgr)
        {
            b.tris.push_back({ ax, ay, abgr });
            b.tris.push_back({ bx, by, abgr });
            b.tris.push_back({ cx, cy, abgr });
        }

        static constexpr int kCircleSegments = 24;

        static void AppendCircle(DebugBatch& b,
                                 float cx, float cy, float r,
                                 uint32_t outlineAbgr, uint32_t fillAbgr,
                                 bool hasFill)
        {
            float prevX = cx + r, prevY = cy;
            for (int i = 1; i <= kCircleSegments; ++i)
            {
                float a = static_cast<float>(i) * (2.0f * bx::kPi / kCircleSegments);
                float nx = cx + r * bx::cos(a);
                float ny = cy + r * bx::sin(a);
                PushLine(b, prevX, prevY, nx, ny, outlineAbgr);
                if (hasFill)
                    PushTri(b, cx, cy, prevX, prevY, nx, ny, fillAbgr);
                prevX = nx; prevY = ny;
            }
        }

        // ---------- eight primitive emitters ----------

        static void EmitCircle(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& c = p.circle2D;
            uint32_t outline = ToABGR(c.outlineColour);
            uint32_t fill    = ToABGR(c.fillColour);
            bool hasFill     = c.fillColour.A() > 0;
            AppendCircle(b, c.position.X(), c.position.Y(), c.radius, outline, fill, hasFill);
        }

        static void EmitLine(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& l = p.line2D;
            PushLine(b, l.start.X(), l.start.Y(), l.end.X(), l.end.Y(), ToABGR(l.colour));
        }

        static void EmitPoint(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& pt = p.point2D;
            b.points.push_back({ pt.position.X(), pt.position.Y(), ToABGR(pt.colour) });
        }

        static void EmitRect(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& r = p.rect2D;
            float minX = r.min.X(), minY = r.min.Y();
            float maxX = r.max.X(), maxY = r.max.Y();
            uint32_t outline = ToABGR(r.outlineColour);
            PushLine(b, minX, minY, maxX, minY, outline);
            PushLine(b, maxX, minY, maxX, maxY, outline);
            PushLine(b, maxX, maxY, minX, maxY, outline);
            PushLine(b, minX, maxY, minX, minY, outline);
            if (r.fillColour.A() > 0)
            {
                uint32_t fill = ToABGR(r.fillColour);
                PushTri(b, minX, minY, maxX, minY, maxX, maxY, fill);
                PushTri(b, minX, minY, maxX, maxY, minX, maxY, fill);
            }
        }

        static void EmitArc(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& a = p.arc2D;
            uint32_t abgr = ToABGR(a.colour);
            float startRad = bx::toRad(a.startAngleDeg);
            float endRad   = bx::toRad(a.endAngleDeg);
            if (endRad < startRad)
                endRad += 2.0f * bx::kPi;
            float span = endRad - startRad;
            int segs = bx::max(2, static_cast<int>(kCircleSegments * span / (2.0f * bx::kPi)));
            float prevX = a.position.X() + a.radius * bx::cos(startRad);
            float prevY = a.position.Y() + a.radius * bx::sin(startRad);
            for (int i = 1; i <= segs; ++i)
            {
                float t = startRad + span * (static_cast<float>(i) / static_cast<float>(segs));
                float nx = a.position.X() + a.radius * bx::cos(t);
                float ny = a.position.Y() + a.radius * bx::sin(t);
                PushLine(b, prevX, prevY, nx, ny, abgr);
                prevX = nx; prevY = ny;
            }
        }

        static void EmitRay(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& r = p.ray2D;
            float ex = r.origin.X() + r.direction.X() * r.length;
            float ey = r.origin.Y() + r.direction.Y() * r.length;
            PushLine(b, r.origin.X(), r.origin.Y(), ex, ey, ToABGR(r.colour));
        }

        static void EmitTriangle(DebugBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& t = p.triangle2D;
            uint32_t outline = ToABGR(t.outlineColour);
            PushLine(b, t.p1.X(), t.p1.Y(), t.p2.X(), t.p2.Y(), outline);
            PushLine(b, t.p2.X(), t.p2.Y(), t.p3.X(), t.p3.Y(), outline);
            PushLine(b, t.p3.X(), t.p3.Y(), t.p1.X(), t.p1.Y(), outline);
            if (t.fillColour.A() > 0)
            {
                uint32_t fill = ToABGR(t.fillColour);
                PushTri(b, t.p1.X(), t.p1.Y(),
                           t.p2.X(), t.p2.Y(),
                           t.p3.X(), t.p3.Y(), fill);
            }
        }

        static void EmitText(const Dia::Graphics::DebugPrimitiveText2D& t,
                             float canvasW, float canvasH,
                             float camX, float camY, float zoom)
        {
            if (t.fontSize <= 0.0f || t.text[0] == '\0')
                return;

            // bgfx debug font is 8x8 characters in screen-space integer coordinates.
            // World → Screen conversion (same math as ViewportTransform::WorldToScreen
            // but inlined here to avoid circular dependency from DiaBgfx → DiaGraphics).
            float screenX = (t.position.X() - camX) * zoom + canvasW * 0.5f;
            float screenY = (t.position.Y() - camY) * zoom + canvasH * 0.5f;
            uint16_t col = static_cast<uint16_t>(bx::clamp(screenX, 0.0f, canvasW) / 8.0f);
            uint16_t row = static_cast<uint16_t>(bx::clamp(screenY, 0.0f, canvasH) / 8.0f);

            uint8_t r = t.colour.R(), g = t.colour.G(), b2 = t.colour.B();
            // Pack a rough 16-colour ANSI code from the dominant channel
            uint8_t attr = 0x0F; // default white on black
            (void)r; (void)g; (void)b2;  // fine-grained colour not available via dbgTextPrintf

            bgfx::dbgTextPrintf(col, row, attr, "%s", t.text);
        }

        // ---------- flush helpers ----------

        static void FlushLines(unsigned short viewId, bgfx::ProgramHandle prog,
                               const std::vector<DebugVertex>& verts)
        {
            if (verts.empty())
                return;
            uint32_t count = static_cast<uint32_t>(verts.size());
            if (bgfx::getAvailTransientVertexBuffer(count, sDebugLayout) < count)
                return;
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, count, sDebugLayout);
            bx::memCopy(tvb.data, verts.data(), sizeof(DebugVertex) * count);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_PT_LINES | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, prog);
        }

        static void FlushTris(unsigned short viewId, bgfx::ProgramHandle prog,
                              const std::vector<DebugVertex>& verts)
        {
            if (verts.empty())
                return;
            uint32_t count = static_cast<uint32_t>(verts.size());
            if (bgfx::getAvailTransientVertexBuffer(count, sDebugLayout) < count)
                return;
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, count, sDebugLayout);
            bx::memCopy(tvb.data, verts.data(), sizeof(DebugVertex) * count);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, prog);
        }

        static void FlushPoints(unsigned short viewId, bgfx::ProgramHandle prog,
                                const std::vector<DebugVertex>& verts)
        {
            if (verts.empty())
                return;
            uint32_t count = static_cast<uint32_t>(verts.size());
            if (bgfx::getAvailTransientVertexBuffer(count, sDebugLayout) < count)
                return;
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, count, sDebugLayout);
            bx::memCopy(tvb.data, verts.data(), sizeof(DebugVertex) * count);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_PT_POINTS | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, prog);
        }

        // ---------- class methods ----------

        DebugRenderer::DebugRenderer(unsigned short viewId, ShaderProgram* debugProgram)
            : mViewId(viewId)
            , mDebugProgram(debugProgram)
            , mCanvasSize(1.0f, 1.0f)
        {
            EnsureDebugLayout();
        }

        DebugRenderer::~DebugRenderer()
        {}

        void DebugRenderer::OnCanvasSizeChanged(const Dia::Maths::Vector2D& size)
        {
            mCanvasSize = size;
        }

        void DebugRenderer::SetCamera(const Dia::Graphics::Camera2D& camera)
        {
            mCamera = camera;
            DIA_LOG_DEBUG("diabgfx", "DebugRenderer: camera pos=(%.1f,%.1f) zoom=%.2f",
                camera.GetPosition().X(), camera.GetPosition().Y(), camera.GetZoom());
        }

        void DebugRenderer::Draw(const Dia::Graphics::DebugFrameData& debug)
        {
            const uint32_t count     = debug.GetDebugPrimitiveCount();
            const uint32_t textCount = debug.GetTextPrimitiveCount();
            if (count == 0 && textCount == 0)
                return;

            if (!mDebugProgram || !mDebugProgram->IsValid())
                return;

            const float cw = mCanvasSize.X();
            const float ch = mCanvasSize.Y();
            const uint16_t w = static_cast<uint16_t>(cw);
            const uint16_t h = static_cast<uint16_t>(ch);

            const float zoom  = mCamera.GetZoom() > 0.0f ? mCamera.GetZoom() : 1.0f;
            const float halfW = (cw * 0.5f) / zoom;
            const float halfH = (ch * 0.5f) / zoom;
            const float cx    = mCamera.GetPosition().X();
            const float cy    = mCamera.GetPosition().Y();

            float ortho[16];
            bx::mtxOrtho(ortho, cx - halfW, cx + halfW, cy + halfH, cy - halfH,
                         0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
            bgfx::setViewTransform(mViewId, nullptr, ortho);
            bgfx::setViewRect(mViewId, 0, 0, w, h);

            bgfx::ProgramHandle prog = { mDebugProgram->GetProgramHandle() };

            DebugBatch batch;
            batch.lines.reserve(count * 8);
            batch.tris.reserve(count * 4);
            batch.points.reserve(count);

            bgfx::setDebug(BGFX_DEBUG_TEXT);

            for (uint32_t i = 0; i < count; ++i)
            {
                const Dia::Graphics::DebugPrimitive& p = debug.GetDebugPrimitive(i);
                switch (p.type)
                {
                    case Dia::Graphics::DebugPrimitiveType::Circle2D:   EmitCircle(batch, p);   break;
                    case Dia::Graphics::DebugPrimitiveType::Line2D:     EmitLine(batch, p);     break;
                    case Dia::Graphics::DebugPrimitiveType::Point2D:    EmitPoint(batch, p);    break;
                    case Dia::Graphics::DebugPrimitiveType::Rect2D:     EmitRect(batch, p);     break;
                    case Dia::Graphics::DebugPrimitiveType::Arc2D:      EmitArc(batch, p);      break;
                    case Dia::Graphics::DebugPrimitiveType::Ray2D:      EmitRay(batch, p);      break;
                    case Dia::Graphics::DebugPrimitiveType::Triangle2D: EmitTriangle(batch, p); break;
                }
            }

            for (uint32_t i = 0; i < textCount; ++i)
                EmitText(debug.GetTextPrimitive(i), cw, ch, cx, cy, zoom);

            FlushLines(mViewId, prog, batch.lines);
            FlushTris(mViewId,  prog, batch.tris);
            FlushPoints(mViewId, prog, batch.points);
        }

    } // namespace Bgfx
} // namespace Dia
