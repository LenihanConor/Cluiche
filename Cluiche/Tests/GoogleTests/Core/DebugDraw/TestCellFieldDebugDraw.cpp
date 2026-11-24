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

namespace
{
    // Overrides the CellField hook to prove virtual dispatch through a base pointer.
    class RecordingDebugDraw : public MinimalDebugDraw
    {
    public:
        void RequestDrawCellField(const CellFieldDrawRequest& r) override { ++mCellFieldCalls; mLast = r; }
        int mCellFieldCalls = 0;
        CellFieldDrawRequest mLast;
    };
}

TEST(CellFieldDebugDraw, GapsDefaultNoOpThroughBasePointerRepeatedly)
{
    MinimalDebugDraw draw;
    IDebugDraw* base = &draw;
    CellFieldDrawRequest request{};
    for (int i = 0; i < 100; ++i)
    {
        base->RequestDrawCellField(request);
    }
    EXPECT_EQ(draw.mCalls, 0);
    EXPECT_EQ(base->DroppedCount(), 0u);
}

TEST(CellFieldDebugDraw, GapsOverrideReceivesRequestThroughBasePointer)
{
    RecordingDebugDraw draw;
    IDebugDraw* base = &draw;
    CellFieldDrawRequest request;
    request.layer.index = 5;
    request.layer.generation = 9;
    request.colourMap = 0xCAFEu;
    base->RequestDrawCellField(request);
    EXPECT_EQ(draw.mCellFieldCalls, 1);
    EXPECT_EQ(draw.mLast.layer.index, 5);
    EXPECT_EQ(draw.mLast.layer.generation, 9);
    EXPECT_EQ(draw.mLast.colourMap, 0xCAFEu);
    EXPECT_EQ(draw.mCalls, 0);
}

TEST(CellFieldDebugDraw, GapsLayerHandleValidity)
{
    CellFieldLayerHandle def;
    EXPECT_FALSE(def.IsValid());
    EXPECT_EQ(def.index, 0xFFFF);
    EXPECT_EQ(def.generation, 0);

    CellFieldLayerHandle zero;
    zero.index = 0;
    EXPECT_TRUE(zero.IsValid());
    zero.index = 0xFFFE;
    EXPECT_TRUE(zero.IsValid());
    zero.index = 0xFFFF;
    EXPECT_FALSE(zero.IsValid());
    EXPECT_EQ(sizeof(CellFieldLayerHandle), 4u);
}

TEST(CellFieldDebugDraw, GapsRequestDefaultsAreInvalidLayersAndNeutral)
{
    CellFieldDrawRequest r;
    EXPECT_FALSE(r.layer.IsValid());
    EXPECT_FALSE(r.elevationLayer.IsValid());
    EXPECT_EQ(r.colourMap, 0u);
    EXPECT_EQ(r.backdropColour, 0u);
    EXPECT_FLOAT_EQ(r.inset, 0.0f);
    EXPECT_FLOAT_EQ(r.hillshadeStrength, 0.0f);
    EXPECT_FLOAT_EQ(r.opacity, 1.0f);
    EXPECT_FLOAT_EQ(r.stateBlend, 1.0f);
    EXPECT_EQ(r.shading, CellFieldShading::Flat);
    EXPECT_TRUE(std::is_trivially_copyable<CellFieldDrawRequest>::value);
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
