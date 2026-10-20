// TestCellFieldDebugDraw.cpp - Google Test unit tests for IDebugDraw CellField hook
//
// Verifies RequestDrawCellField is a non-breaking, default no-op extension.

#include <gtest/gtest.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

using namespace Dia;
using namespace Dia::Core;

namespace
{
    // Implements only the pure virtuals of IDebugDraw.
    class MinimalDebugDraw : public IDebugDraw
    {
    public:
        using IDebugDraw::RequestDraw;

        void RequestDraw(const Maths::Vector2D&, float, RGBA, RGBA) override { ++mCalls; }
        void RequestDraw(const Maths::Vector2D&, const Maths::Vector2D&, RGBA) override { ++mCalls; }
        void RequestDrawPoint(const Maths::Vector2D&, RGBA) override { ++mCalls; }
        void RequestDrawRect(const Maths::Vector2D&, const Maths::Vector2D&, RGBA, RGBA) override { ++mCalls; }
        void RequestDrawArc(const Maths::Vector2D&, float, float, float, RGBA) override { ++mCalls; }
        void RequestDrawRay(const Maths::Vector2D&, const Maths::Vector2D&, float, RGBA) override { ++mCalls; }
        void RequestDraw(const Maths::Vector2D&, const Maths::Vector2D&, const Maths::Vector2D&, RGBA, RGBA) override { ++mCalls; }
        void RequestDrawText(const Maths::Vector2D&, const char*, float, RGBA) override { ++mCalls; }
        void RequestDrawLine3D(const Maths::Vector3D&, const Maths::Vector3D&, RGBA) override { ++mCalls; }
        void RequestDrawRay3D(const Maths::Vector3D&, const Maths::Vector3D&, float, RGBA) override { ++mCalls; }
        void RequestDrawBox3D(const Maths::Vector3D&, const Maths::Vector3D&, RGBA) override { ++mCalls; }
        void RequestDrawSphere3D(const Maths::Vector3D&, float, RGBA) override { ++mCalls; }
        void RequestDrawArrow3D(const Maths::Vector3D&, const Maths::Vector3D&, float, float, RGBA) override { ++mCalls; }
        uint32_t DroppedCount() const override { return 0; }
        const Maths::Vector2D& GetMousePixel() const override { return mMouse; }

        int mCalls = 0;
        Maths::Vector2D mMouse;
    };
}

TEST(CellFieldDebugDraw, TestDebugDrawCellFieldDefaultNoOp)
{
    MinimalDebugDraw draw;
    IDebugDraw& iface = draw;

    CellFieldDrawRequest request{};
    iface.RequestDrawCellField(request);

    EXPECT_EQ(draw.mCalls, 0);
    EXPECT_EQ(iface.DroppedCount(), 0u);
}
