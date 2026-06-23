////////////////////////////////////////////////////////////////////////////////
// Filename: DebugGeometry3DRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Renderers/DebugGeometry3DRenderer.h"
#include <DiaBgfx/Resources/ShaderProgram.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <vector>

namespace Dia
{
    namespace Bgfx3D
    {
        // ---------- vertex layout ----------

        struct Debug3DVertex
        {
            float    x, y, z;
            uint32_t abgr;
        };

        static bgfx::VertexLayout sDebug3DLayout;
        static bool               sDebug3DLayoutInit = false;

        static void EnsureDebug3DLayout()
        {
            if (sDebug3DLayoutInit)
                return;
            sDebug3DLayout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
                .end();
            sDebug3DLayoutInit = true;
        }

        // ---------- colour helper ----------

        static uint32_t ToABGR(const Dia::Graphics::RGBA& c)
        {
            return (static_cast<uint32_t>(c.A()) << 24) |
                   (static_cast<uint32_t>(c.B()) << 16) |
                   (static_cast<uint32_t>(c.G()) <<  8) |
                   (static_cast<uint32_t>(c.R()));
        }

        // ---------- batch accumulator ----------

        using Debug3DBatch = std::vector<Debug3DVertex>;

        // ---------- helpers ----------

        static void PushLine3D(Debug3DBatch& b,
                               float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               uint32_t abgr)
        {
            b.push_back({ x0, y0, z0, abgr });
            b.push_back({ x1, y1, z1, abgr });
        }

        // ---------- primitive emitters ----------

        static void EmitLine3D(Debug3DBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& l = p.line3D;
            uint32_t abgr = ToABGR(l.colour);
            PushLine3D(b, l.from.X(), l.from.Y(), l.from.Z(),
                          l.to.X(),   l.to.Y(),   l.to.Z(), abgr);
        }

        static void EmitRay3D(Debug3DBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& r = p.ray3D;
            uint32_t abgr = ToABGR(r.colour);
            float ex = r.origin.X() + r.direction.X() * r.length;
            float ey = r.origin.Y() + r.direction.Y() * r.length;
            float ez = r.origin.Z() + r.direction.Z() * r.length;
            PushLine3D(b, r.origin.X(), r.origin.Y(), r.origin.Z(), ex, ey, ez, abgr);

            // Arrowhead: two lines in a plane perpendicular to direction
            Dia::Maths::Vector3D dir(r.direction.X(), r.direction.Y(), r.direction.Z());
            Dia::Maths::Vector3D up(0.0f, 1.0f, 0.0f);
            if (bx::abs(dir.Dot(up)) > 0.9f)
                up = Dia::Maths::Vector3D(1.0f, 0.0f, 0.0f);
            Dia::Maths::Vector3D perp = dir.Cross(up).AsNormal();

            constexpr float kArrowLen = 0.15f;
            constexpr float kCos30    = 0.866f;
            constexpr float kSin30    = 0.5f;
            float alen = r.length * kArrowLen;

            float bx2 = -dir.X() * kCos30 + perp.X() * kSin30;
            float by2 = -dir.Y() * kCos30 + perp.Y() * kSin30;
            float bz2 = -dir.Z() * kCos30 + perp.Z() * kSin30;
            float cx2 = -dir.X() * kCos30 - perp.X() * kSin30;
            float cy2 = -dir.Y() * kCos30 - perp.Y() * kSin30;
            float cz2 = -dir.Z() * kCos30 - perp.Z() * kSin30;
            PushLine3D(b, ex, ey, ez, ex + bx2*alen, ey + by2*alen, ez + bz2*alen, abgr);
            PushLine3D(b, ex, ey, ez, ex + cx2*alen, ey + cy2*alen, ez + cz2*alen, abgr);
        }

        static void EmitBox3D(Debug3DBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& box = p.box3D;
            uint32_t abgr = ToABGR(box.colour);
            float x0 = box.min.X(), y0 = box.min.Y(), z0 = box.min.Z();
            float x1 = box.max.X(), y1 = box.max.Y(), z1 = box.max.Z();

            // Bottom face
            PushLine3D(b, x0,y0,z0, x1,y0,z0, abgr);
            PushLine3D(b, x1,y0,z0, x1,y0,z1, abgr);
            PushLine3D(b, x1,y0,z1, x0,y0,z1, abgr);
            PushLine3D(b, x0,y0,z1, x0,y0,z0, abgr);
            // Top face
            PushLine3D(b, x0,y1,z0, x1,y1,z0, abgr);
            PushLine3D(b, x1,y1,z0, x1,y1,z1, abgr);
            PushLine3D(b, x1,y1,z1, x0,y1,z1, abgr);
            PushLine3D(b, x0,y1,z1, x0,y1,z0, abgr);
            // Vertical edges
            PushLine3D(b, x0,y0,z0, x0,y1,z0, abgr);
            PushLine3D(b, x1,y0,z0, x1,y1,z0, abgr);
            PushLine3D(b, x1,y0,z1, x1,y1,z1, abgr);
            PushLine3D(b, x0,y0,z1, x0,y1,z1, abgr);
        }

        static constexpr int kSphereSegments = 24;

        static void EmitSphere3D(Debug3DBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& s = p.sphere3D;
            uint32_t abgr = ToABGR(s.colour);
            float cx = s.center.X(), cy = s.center.Y(), cz = s.center.Z(), r = s.radius;

            for (int seg = 0; seg < kSphereSegments; ++seg)
            {
                float a0 = static_cast<float>(seg)     * (2.0f * bx::kPi / kSphereSegments);
                float a1 = static_cast<float>(seg + 1) * (2.0f * bx::kPi / kSphereSegments);
                float c0 = bx::cos(a0), s0 = bx::sin(a0);
                float c1 = bx::cos(a1), s1 = bx::sin(a1);
                // XY plane
                PushLine3D(b, cx+r*c0, cy+r*s0, cz, cx+r*c1, cy+r*s1, cz, abgr);
                // XZ plane
                PushLine3D(b, cx+r*c0, cy, cz+r*s0, cx+r*c1, cy, cz+r*s1, abgr);
                // YZ plane
                PushLine3D(b, cx, cy+r*c0, cz+r*s0, cx, cy+r*c1, cz+r*s1, abgr);
            }
        }

        static void EmitArrow3D(Debug3DBatch& b, const Dia::Graphics::DebugPrimitive& p)
        {
            const auto& a = p.arrow3D;
            uint32_t abgr = ToABGR(a.colour);

            float ex = a.origin.X() + a.direction.X() * a.length;
            float ey = a.origin.Y() + a.direction.Y() * a.length;
            float ez = a.origin.Z() + a.direction.Z() * a.length;

            // Shaft
            PushLine3D(b, a.origin.X(), a.origin.Y(), a.origin.Z(), ex, ey, ez, abgr);

            // Cone base center (back along direction from tip)
            float coneLen = a.length * a.headSize;
            float bcx = ex - a.direction.X() * coneLen;
            float bcy = ey - a.direction.Y() * coneLen;
            float bcz = ez - a.direction.Z() * coneLen;

            // Build 2 perpendiculars to direction
            Dia::Maths::Vector3D dir(a.direction.X(), a.direction.Y(), a.direction.Z());
            Dia::Maths::Vector3D up(0.0f, 1.0f, 0.0f);
            if (bx::abs(dir.Dot(up)) > 0.9f)
                up = Dia::Maths::Vector3D(1.0f, 0.0f, 0.0f);
            Dia::Maths::Vector3D perp1 = dir.Cross(up).AsNormal();
            Dia::Maths::Vector3D perp2 = dir.Cross(perp1);

            float r = a.length * a.headSize;  // cone base radius
            // 4 base points at ±perp1, ±perp2
            float pts[4][3] = {
                { bcx + perp1.X()*r, bcy + perp1.Y()*r, bcz + perp1.Z()*r },
                { bcx - perp1.X()*r, bcy - perp1.Y()*r, bcz - perp1.Z()*r },
                { bcx + perp2.X()*r, bcy + perp2.Y()*r, bcz + perp2.Z()*r },
                { bcx - perp2.X()*r, bcy - perp2.Y()*r, bcz - perp2.Z()*r },
            };
            for (int i = 0; i < 4; ++i)
                PushLine3D(b, ex, ey, ez, pts[i][0], pts[i][1], pts[i][2], abgr);
        }

        // ---------- class methods ----------

        DebugGeometry3DRenderer::DebugGeometry3DRenderer(unsigned short viewId,
                                                         Dia::Bgfx::ShaderProgram* debugProgram)
            : mViewId(viewId)
            , mDebugProgram(debugProgram)
        {
            EnsureDebug3DLayout();
        }

        DebugGeometry3DRenderer::~DebugGeometry3DRenderer()
        {}

        void DebugGeometry3DRenderer::Draw(
            const Dia::Graphics::DebugFrameData& debugData,
            const Dia::Graphics3D::Camera3D& camera,
            const Dia::Maths::Vector2D& canvasSize)
        {
            const uint32_t count = debugData.GetDebug3DPrimitiveCount();
            if (count == 0)
                return;

            if (!mDebugProgram || !mDebugProgram->IsValid())
                return;

            const uint16_t w = static_cast<uint16_t>(canvasSize.X());
            const uint16_t h = static_cast<uint16_t>(canvasSize.Y());

            float viewMtx[16], projMtx[16];
            camera.view.GetColumnMajor(viewMtx);
            camera.projection.GetColumnMajor(projMtx);
            bgfx::setViewTransform(mViewId, viewMtx, projMtx);
            bgfx::setViewRect(mViewId, 0, 0, w > 0 ? w : 1280, h > 0 ? h : 720);
            // No clear — overlay on top of mesh pass

            bgfx::ProgramHandle prog = { mDebugProgram->GetProgramHandle() };

            Debug3DBatch batch;
            batch.reserve(count * 24);  // worst case Box3D

            for (uint32_t i = 0; i < count; ++i)
            {
                const Dia::Graphics::DebugPrimitive& p = debugData.GetDebug3DPrimitive(i);
                switch (p.type)
                {
                    case Dia::Graphics::DebugPrimitiveType::Line3D:   EmitLine3D(batch, p);   break;
                    case Dia::Graphics::DebugPrimitiveType::Ray3D:    EmitRay3D(batch, p);    break;
                    case Dia::Graphics::DebugPrimitiveType::Box3D:    EmitBox3D(batch, p);    break;
                    case Dia::Graphics::DebugPrimitiveType::Sphere3D: EmitSphere3D(batch, p); break;
                    case Dia::Graphics::DebugPrimitiveType::Arrow3D:  EmitArrow3D(batch, p);  break;
                    default: break;
                }
            }

            if (batch.empty())
                return;

            uint32_t vcount = static_cast<uint32_t>(batch.size());
            if (bgfx::getAvailTransientVertexBuffer(vcount, sDebug3DLayout) < vcount)
                return;

            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, vcount, sDebug3DLayout);
            bx::memCopy(tvb.data, batch.data(), sizeof(Debug3DVertex) * vcount);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_PT_LINES | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(mViewId, prog);
        }

    } // namespace Bgfx3D
} // namespace Dia
