#include <gtest/gtest.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/EntityFrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaGraphics/Frame/EntityFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

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
// DebugFrameData — basic buffer tests
// ===========================================================================

TEST(DiaGraphics_FrameData, DebugFrameData_DefaultConstruction_Empty)
{
	DebugFrameData dfd;
	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.TotalCount(), 0);
}

TEST(DiaGraphics_FrameData, DebugFrameData_RequestDrawCircle_VisitorReceivesIt)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(1.0f, 2.0f), 3.0f, RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.CircleCount(), 1);
}

TEST(DiaGraphics_FrameData, DebugFrameData_RequestDrawLine_VisitorReceivesIt)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f), RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.LineCount(), 1);
}

TEST(DiaGraphics_FrameData, DebugFrameData_ClearDebugBuffer_RemovesAll)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f), RGBA::White);
	dfd.ClearDebugBuffer();

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.TotalCount(), 0);
}

TEST(DiaGraphics_FrameData, DebugFrameData_MultipleCirclesAndLines)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);
	dfd.RequestDraw(Vector2D(1.0f, 1.0f), 2.0f, RGBA::Red);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f), RGBA::White);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(0.0f, 1.0f), RGBA::White);
	dfd.RequestDraw(Vector2D(1.0f, 1.0f), Vector2D(2.0f, 2.0f), RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.CircleCount(), 2);
	EXPECT_EQ(v.LineCount(),   3);
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
	fd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);

	fd.Clear();

	EXPECT_EQ(fd.GetSprites().Size(), 0u);

	RecordingDebugVisitor v;
	static_cast<DebugFrameData&>(fd).AcceptVisitor(v);
	EXPECT_EQ(v.TotalCount(), 0);
}

TEST(DiaGraphics_FrameData, FrameData_CopyPreservesData)
{
	FrameData src;
	src.RequestDrawSprite(SpriteDrawCommand(7u, Vector2D(1.0f, 2.0f)));
	src.RequestDraw(Vector2D(3.0f, 4.0f), 5.0f, RGBA::White);

	FrameData dst;
	dst.Copy(src);

	EXPECT_EQ(dst.GetSprites().Size(), 1u);
	EXPECT_EQ(dst.GetSprites().At(0).textureId, 7u);

	RecordingDebugVisitor v;
	static_cast<DebugFrameData&>(dst).AcceptVisitor(v);
	EXPECT_EQ(v.CircleCount(), 1);
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
// Per-primitive type tag tests
// ===========================================================================

TEST(DiaGraphics_DebugPrimitive, RequestDrawCircle2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(1.0f, 2.0f), 3.0f, RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.CircleCount(), 1);
	EXPECT_EQ(v.TotalCount(),  1);
}

TEST(DiaGraphics_DebugPrimitive, RequestDrawLine2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f), RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.LineCount(),  1);
	EXPECT_EQ(v.TotalCount(), 1);
}

TEST(DiaGraphics_DebugPrimitive, RequestDrawPoint2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDrawPoint(Vector2D(5.0f, 5.0f), RGBA::Red);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.PointCount(), 1);
	EXPECT_EQ(v.TotalCount(), 1);
}

TEST(DiaGraphics_DebugPrimitive, RequestDrawRect2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDrawRect(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f), RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.RectCount(),  1);
	EXPECT_EQ(v.TotalCount(), 1);
}

TEST(DiaGraphics_DebugPrimitive, RequestDrawArc2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDrawArc(Vector2D(0.0f, 0.0f), 10.0f, 0.0f, 90.0f, RGBA::Blue);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.ArcCount(),   1);
	EXPECT_EQ(v.TotalCount(), 1);
}

TEST(DiaGraphics_DebugPrimitive, RequestDrawRay2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDrawRay(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f), 50.0f, RGBA::Green);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.RayCount(),   1);
	EXPECT_EQ(v.TotalCount(), 1);
}

TEST(DiaGraphics_DebugPrimitive, RequestDrawTriangle2D_CorrectTag)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 10.0f), Vector2D(10.0f, 0.0f), RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.TriangleCount(), 1);
	EXPECT_EQ(v.TotalCount(),    1);
}

// ===========================================================================
// Insertion order test
// ===========================================================================

TEST(DiaGraphics_DebugPrimitive, InsertionOrderPreserved)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);                              // Circle
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f), RGBA::White);              // Line
	dfd.RequestDrawPoint(Vector2D(2.0f, 2.0f), RGBA::Red);                                  // Point

	OrderRecordingDebugVisitor v;
	dfd.AcceptVisitor(v);

	ASSERT_EQ(v.count, 3);
	EXPECT_EQ(v.order[0], DebugPrimitiveType::Circle2D);
	EXPECT_EQ(v.order[1], DebugPrimitiveType::Line2D);
	EXPECT_EQ(v.order[2], DebugPrimitiveType::Point2D);
}

// ===========================================================================
// Fill colour tests
// ===========================================================================

TEST(DiaGraphics_DebugPrimitive, FillColourDefault_IsTransparent)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 5.0f, RGBA::White);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	ASSERT_EQ(v.visitCount, 1);
	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Circle2D);
	EXPECT_EQ(v.lastPrimitive.circle2D.fillColour.A(), 0u);
}

TEST(DiaGraphics_DebugPrimitive, FillColourExplicit_StoredCorrectly)
{
	DebugFrameData dfd;
	RGBA fill(255, 0, 0, 128);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 5.0f, RGBA::White, fill);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	ASSERT_EQ(v.visitCount, 1);
	EXPECT_EQ(v.lastPrimitive.circle2D.fillColour.R(), 255u);
	EXPECT_EQ(v.lastPrimitive.circle2D.fillColour.A(), 128u);
}

// ===========================================================================
// Per-shape field storage tests
// ===========================================================================

TEST(DiaGraphics_DebugPrimitive, Circle2D_FieldsStoredCorrectly)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(1.5f, 2.5f), 4.0f, RGBA::Red);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Circle2D);
	EXPECT_FLOAT_EQ(v.lastPrimitive.circle2D.position.x, 1.5f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.circle2D.position.y, 2.5f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.circle2D.radius, 4.0f);
}

TEST(DiaGraphics_DebugPrimitive, Line2D_FieldsStoredCorrectly)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(1.0f, 2.0f), Vector2D(3.0f, 4.0f), RGBA::Green);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Line2D);
	EXPECT_FLOAT_EQ(v.lastPrimitive.line2D.start.x, 1.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.line2D.end.y,   4.0f);
}

TEST(DiaGraphics_DebugPrimitive, Rect2D_FieldsStoredCorrectly)
{
	DebugFrameData dfd;
	RGBA fill(0, 255, 0, 200);
	dfd.RequestDrawRect(Vector2D(1.0f, 2.0f), Vector2D(11.0f, 12.0f), RGBA::White, fill);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Rect2D);
	EXPECT_FLOAT_EQ(v.lastPrimitive.rect2D.min.x, 1.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.rect2D.max.y, 12.0f);
	EXPECT_EQ(v.lastPrimitive.rect2D.fillColour.G(), 255u);
}

TEST(DiaGraphics_DebugPrimitive, Triangle2D_FieldsStoredCorrectly)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 10.0f), Vector2D(10.0f, 0.0f), RGBA::Blue);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Triangle2D);
	EXPECT_FLOAT_EQ(v.lastPrimitive.triangle2D.p2.x, 5.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.triangle2D.p2.y, 10.0f);
}

TEST(DiaGraphics_DebugPrimitive, Arc2D_FieldsStoredCorrectly)
{
	DebugFrameData dfd;
	dfd.RequestDrawArc(Vector2D(3.0f, 4.0f), 7.0f, 30.0f, 120.0f, RGBA::Cyan);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Arc2D);
	EXPECT_FLOAT_EQ(v.lastPrimitive.arc2D.position.x,    3.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.arc2D.radius,         7.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.arc2D.startAngleDeg, 30.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.arc2D.endAngleDeg,  120.0f);
}

TEST(DiaGraphics_DebugPrimitive, Ray2D_FieldsStoredCorrectly)
{
	DebugFrameData dfd;
	dfd.RequestDrawRay(Vector2D(1.0f, 2.0f), Vector2D(1.0f, 0.0f), 50.0f, RGBA::Yellow);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Ray2D);
	EXPECT_FLOAT_EQ(v.lastPrimitive.ray2D.origin.x,      1.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.ray2D.direction.x,   1.0f);
	EXPECT_FLOAT_EQ(v.lastPrimitive.ray2D.length,        50.0f);
}

// ===========================================================================
// Mixed types, copy, and clear tests
// ===========================================================================

TEST(DiaGraphics_DebugPrimitive, MixedTypes_AllCounted)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f), RGBA::White);
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f), RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.CircleCount(), 2);
	EXPECT_EQ(v.LineCount(),   2);
	EXPECT_EQ(v.TotalCount(),  4);
}

TEST(DiaGraphics_DebugPrimitive, CopyPreservesAllPrimitives)
{
	DebugFrameData src;
	src.RequestDraw(Vector2D(1.0f, 1.0f), 3.0f, RGBA::Red);
	src.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f), RGBA::White);
	src.RequestDrawPoint(Vector2D(5.0f, 5.0f), RGBA::Blue);

	DebugFrameData dst;
	dst.CopyDebugBuffer(src);

	OrderRecordingDebugVisitor v;
	dst.AcceptVisitor(v);

	ASSERT_EQ(v.count, 3);
	EXPECT_EQ(v.order[0], DebugPrimitiveType::Circle2D);
	EXPECT_EQ(v.order[1], DebugPrimitiveType::Line2D);
	EXPECT_EQ(v.order[2], DebugPrimitiveType::Point2D);
}

TEST(DiaGraphics_DebugPrimitive, ClearThenAdd_CorrectCount)
{
	DebugFrameData dfd;
	for (int i = 0; i < 5; ++i)
		dfd.RequestDraw(Vector2D(0.0f, 0.0f), 1.0f, RGBA::White);

	dfd.ClearDebugBuffer();

	dfd.RequestDrawPoint(Vector2D(0.0f, 0.0f), RGBA::Red);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.TotalCount(), 1);
	EXPECT_EQ(v.PointCount(), 1);
}

// ===========================================================================
// Exhaustive / boundary tests
// ===========================================================================

TEST(DiaGraphics_DebugPrimitive, CapacityBoundary_Push1024_AllStored)
{
	DebugFrameData dfd;
	for (int i = 0; i < 1024; ++i)
		dfd.RequestDraw(Vector2D(static_cast<float>(i), 0.0f), 1.0f, RGBA::White);

	RecordingDebugVisitor v;
	dfd.AcceptVisitor(v);
	EXPECT_EQ(v.CircleCount(), 1024);
	EXPECT_EQ(v.TotalCount(),  1024);
}

TEST(DiaGraphics_DebugPrimitive, SelfAssignment_DoesNotCorrupt)
{
	DebugPrimitive p;
	p.type             = DebugPrimitiveType::Circle2D;
	p.circle2D.radius  = 7.0f;
	p.circle2D.position = Vector2D(1.0f, 2.0f);
	p.circle2D.outlineColour = RGBA::Red;
	p.circle2D.fillColour    = RGBA(0, 0, 0, 0);

	p = p;  // self-assign

	EXPECT_EQ(p.type, DebugPrimitiveType::Circle2D);
	EXPECT_FLOAT_EQ(p.circle2D.radius, 7.0f);
}

TEST(DiaGraphics_DebugPrimitive, CopyBuffer_IndependentFromSource)
{
	DebugFrameData src;
	src.RequestDraw(Vector2D(1.0f, 1.0f), 3.0f, RGBA::Red);
	src.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f), RGBA::White);

	DebugFrameData dst;
	dst.CopyDebugBuffer(src);

	// Clear the source — dst must be unaffected
	src.ClearDebugBuffer();

	RecordingDebugVisitor srcV;
	src.AcceptVisitor(srcV);
	EXPECT_EQ(srcV.TotalCount(), 0);

	RecordingDebugVisitor dstV;
	dst.AcceptVisitor(dstV);
	EXPECT_EQ(dstV.CircleCount(), 1);
	EXPECT_EQ(dstV.LineCount(),   1);
	EXPECT_EQ(dstV.TotalCount(),  2);
}

TEST(DiaGraphics_DebugPrimitive, FillColourDefault_Rect2D_IsTransparent)
{
	DebugFrameData dfd;
	dfd.RequestDrawRect(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f), RGBA::White);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	ASSERT_EQ(v.visitCount, 1);
	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Rect2D);
	EXPECT_EQ(v.lastPrimitive.rect2D.fillColour.A(), 0u);
}

TEST(DiaGraphics_DebugPrimitive, FillColourDefault_Triangle2D_IsTransparent)
{
	DebugFrameData dfd;
	dfd.RequestDraw(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 10.0f), Vector2D(10.0f, 0.0f), RGBA::White);

	InspectingDebugVisitor v;
	dfd.AcceptVisitor(v);

	ASSERT_EQ(v.visitCount, 1);
	EXPECT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Triangle2D);
	EXPECT_EQ(v.lastPrimitive.triangle2D.fillColour.A(), 0u);
}
