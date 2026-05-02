#include <gtest/gtest.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/EntityFrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaGraphics/Frame/DebugFrameDataCircle2D.h>
#include <DiaGraphics/Frame/DebugFrameDataLine2D.h>
#include <DiaGraphics/Frame/EntityFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Mock visitors for recording visit calls
// ---------------------------------------------------------------------------

struct RecordingEntityVisitor : public EntityFrameDataVisitor
{
    mutable int visitCount = 0;
    void Visit(const EntityFrameData& data) const override { ++visitCount; }
};

struct RecordingDebugVisitor : public DebugFrameDataVisitor
{
    mutable int circleCount = 0;
    mutable int lineCount   = 0;
    mutable int frameCount  = 0;

    void Visit(const DebugFrameDataCircle2D& obj) const override { ++circleCount; }
    void Visit(const DebugFrameDataLine2D& obj)   const override { ++lineCount;   }
    void Visit(const DebugFrameData& data)         const override { ++frameCount; }
};

// ===========================================================================
// SpriteDrawCommand tests
// ===========================================================================

TEST(DiaGraphics_FrameData, SpriteDrawCommand_DefaultConstruction_Sane)
{
    SpriteDrawCommand cmd;
    EXPECT_EQ(cmd.textureId, 0u);
    EXPECT_FLOAT_EQ(cmd.rotation, 0.0f);
    EXPECT_EQ(cmd.layer, 0);
}

TEST(DiaGraphics_FrameData, SpriteDrawCommand_ConstructWithIdAndPos_StoresValues)
{
    Vector2D pos(3.5f, -2.0f);
    SpriteDrawCommand cmd(42u, pos);

    EXPECT_EQ(cmd.textureId, 42u);
    EXPECT_FLOAT_EQ(cmd.position.x, 3.5f);
    EXPECT_FLOAT_EQ(cmd.position.y, -2.0f);
}

// ===========================================================================
// EntityFrameData tests
// ===========================================================================

TEST(DiaGraphics_FrameData, EntityFrameData_DefaultConstruction_Empty)
{
    EntityFrameData efd;
    EXPECT_EQ(efd.GetSprites().Size(), 0u);
}

TEST(DiaGraphics_FrameData, EntityFrameData_RequestDrawSprite_IncrementsCount)
{
    EntityFrameData efd;
    efd.RequestDrawSprite(SpriteDrawCommand(1u, Vector2D(0.0f, 0.0f)));
    EXPECT_EQ(efd.GetSprites().Size(), 1u);
}

TEST(DiaGraphics_FrameData, EntityFrameData_MultipleSprites_AllStored)
{
    EntityFrameData efd;
    for (unsigned int i = 0; i < 5; ++i)
        efd.RequestDrawSprite(SpriteDrawCommand(i, Vector2D(static_cast<float>(i), 0.0f)));

    EXPECT_EQ(efd.GetSprites().Size(), 5u);
    for (unsigned int i = 0; i < 5; ++i)
        EXPECT_EQ(efd.GetSprites().At(i).textureId, i);
}

TEST(DiaGraphics_FrameData, EntityFrameData_Clear_RemovesAllSprites)
{
    EntityFrameData efd;
    efd.RequestDrawSprite(SpriteDrawCommand(1u, Vector2D(0.0f, 0.0f)));
    efd.RequestDrawSprite(SpriteDrawCommand(2u, Vector2D(1.0f, 0.0f)));
    EXPECT_EQ(efd.GetSprites().Size(), 2u);

    efd.Clear();
    EXPECT_EQ(efd.GetSprites().Size(), 0u);
}

TEST(DiaGraphics_FrameData, EntityFrameData_AcceptVisitor_VisitorCalled)
{
    EntityFrameData efd;
    efd.RequestDrawSprite(SpriteDrawCommand(1u, Vector2D(0.0f, 0.0f)));

    RecordingEntityVisitor visitor;
    efd.AcceptVisitor(visitor);
    EXPECT_EQ(visitor.visitCount, 1);
}

TEST(DiaGraphics_FrameData, EntityFrameData_ClearThenAdd_SizeCorrect)
{
    EntityFrameData efd;
    for (int i = 0; i < 10; ++i)
        efd.RequestDrawSprite(SpriteDrawCommand(static_cast<unsigned int>(i), Vector2D()));
    efd.Clear();
    efd.RequestDrawSprite(SpriteDrawCommand(99u, Vector2D()));
    EXPECT_EQ(efd.GetSprites().Size(), 1u);
    EXPECT_EQ(efd.GetSprites().At(0).textureId, 99u);
}

// ===========================================================================
// DebugFrameData tests
// ===========================================================================

TEST(DiaGraphics_FrameData, DebugFrameData_DefaultConstruction_Empty)
{
    DebugFrameData dfd;
    RecordingDebugVisitor v;
    // Visit the frame — circles and lines should be zero
    dfd.AcceptVisitor(v);
    EXPECT_EQ(v.circleCount, 0);
    EXPECT_EQ(v.lineCount,   0);
}

TEST(DiaGraphics_FrameData, DebugFrameData_RequestDrawCircle_VisitorReceivesIt)
{
    DebugFrameData dfd;
    dfd.RequestDraw(DebugFrameDataCircle2D(Vector2D(1.0f, 2.0f), 3.0f));

    RecordingDebugVisitor v;
    dfd.AcceptVisitor(v);
    EXPECT_EQ(v.circleCount, 1);
}

TEST(DiaGraphics_FrameData, DebugFrameData_RequestDrawLine_VisitorReceivesIt)
{
    DebugFrameData dfd;
    dfd.RequestDraw(DebugFrameDataLine2D(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f)));

    RecordingDebugVisitor v;
    dfd.AcceptVisitor(v);
    EXPECT_EQ(v.lineCount, 1);
}

TEST(DiaGraphics_FrameData, DebugFrameData_ClearDebugBuffer_RemovesAll)
{
    DebugFrameData dfd;
    dfd.RequestDraw(DebugFrameDataCircle2D(Vector2D(0.0f, 0.0f), 1.0f));
    dfd.RequestDraw(DebugFrameDataLine2D(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f)));
    dfd.ClearDebugBuffer();

    RecordingDebugVisitor v;
    dfd.AcceptVisitor(v);
    EXPECT_EQ(v.circleCount, 0);
    EXPECT_EQ(v.lineCount,   0);
}

TEST(DiaGraphics_FrameData, DebugFrameData_MultipleCirclesAndLines)
{
    DebugFrameData dfd;
    dfd.RequestDraw(DebugFrameDataCircle2D(Vector2D(0.0f, 0.0f), 1.0f));
    dfd.RequestDraw(DebugFrameDataCircle2D(Vector2D(1.0f, 1.0f), 2.0f));
    dfd.RequestDraw(DebugFrameDataLine2D(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f)));
    dfd.RequestDraw(DebugFrameDataLine2D(Vector2D(0.0f, 0.0f), Vector2D(0.0f, 1.0f)));
    dfd.RequestDraw(DebugFrameDataLine2D(Vector2D(1.0f, 1.0f), Vector2D(2.0f, 2.0f)));

    RecordingDebugVisitor v;
    dfd.AcceptVisitor(v);
    EXPECT_EQ(v.circleCount, 2);
    EXPECT_EQ(v.lineCount,   3);
}

// ===========================================================================
// FrameData (combined) tests
// ===========================================================================

TEST(DiaGraphics_FrameData, FrameData_DefaultConstruction_Empty)
{
    FrameData fd;
    EXPECT_EQ(fd.GetSprites().Size(), 0u);
}

TEST(DiaGraphics_FrameData, FrameData_Clear_ClearsBothEntityAndDebug)
{
    FrameData fd;
    fd.RequestDrawSprite(SpriteDrawCommand(1u, Vector2D(0.0f, 0.0f)));
    fd.RequestDraw(DebugFrameDataCircle2D(Vector2D(0.0f, 0.0f), 1.0f));

    fd.Clear();

    EXPECT_EQ(fd.GetSprites().Size(), 0u);

    RecordingDebugVisitor v;
    static_cast<DebugFrameData&>(fd).AcceptVisitor(v);
    EXPECT_EQ(v.circleCount, 0);
}

TEST(DiaGraphics_FrameData, FrameData_CopyPreservesData)
{
    FrameData src;
    src.RequestDrawSprite(SpriteDrawCommand(7u, Vector2D(1.0f, 2.0f)));
    src.RequestDraw(DebugFrameDataCircle2D(Vector2D(3.0f, 4.0f), 5.0f));

    FrameData dst;
    dst.Copy(src);

    EXPECT_EQ(dst.GetSprites().Size(), 1u);
    EXPECT_EQ(dst.GetSprites().At(0).textureId, 7u);

    RecordingDebugVisitor v;
    static_cast<DebugFrameData&>(dst).AcceptVisitor(v);
    EXPECT_EQ(v.circleCount, 1);
}

TEST(DiaGraphics_FrameData, FrameData_AssignmentPreservesData)
{
    FrameData src;
    src.RequestDrawSprite(SpriteDrawCommand(3u, Vector2D(0.0f, 0.0f)));

    FrameData dst;
    dst = src;

    EXPECT_EQ(dst.GetSprites().Size(), 1u);
    EXPECT_EQ(dst.GetSprites().At(0).textureId, 3u);
}

// ===========================================================================
// DebugFrameDataCircle2D / Line2D accessors
// ===========================================================================

TEST(DiaGraphics_FrameData, DebugCircle2D_Accessors_ReturnConstructedValues)
{
    Vector2D pos(1.5f, 2.5f);
    float    radius = 4.0f;
    RGBA     colour = RGBA::White;

    DebugFrameDataCircle2D circle(pos, radius, colour);

    EXPECT_FLOAT_EQ(circle.GetPosition().x, pos.x);
    EXPECT_FLOAT_EQ(circle.GetPosition().y, pos.y);
    EXPECT_FLOAT_EQ(circle.GetRadius(), radius);
}

TEST(DiaGraphics_FrameData, DebugLine2D_Accessors_ReturnConstructedValues)
{
    Vector2D p1(0.0f, 0.0f);
    Vector2D p2(10.0f, 5.0f);

    DebugFrameDataLine2D line(p1, p2);

    EXPECT_FLOAT_EQ(line.GetPosition1().x, p1.x);
    EXPECT_FLOAT_EQ(line.GetPosition1().y, p1.y);
    EXPECT_FLOAT_EQ(line.GetPosition2().x, p2.x);
    EXPECT_FLOAT_EQ(line.GetPosition2().y, p2.y);
}
