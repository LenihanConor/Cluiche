// TestDebugTextPrimitive.cpp - Google Test unit tests for the Text2D debug primitive
//
// Tests DebugPrimitiveText2D struct and DebugFrameData::RequestDrawText() submission logic.
// Text primitives live in a separate buffer (mTextBuffer) independent of geometry.

#include <gtest/gtest.h>

#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstring>

using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

// ===========================================================================
// Helpers
// ===========================================================================

namespace
{
	static const Vector2D kZero(0.0f, 0.0f);
	static const Vector2D kPos(10.0f, 20.0f);
	static const RGBA     kCyan  = RGBA::Cyan;
	static const RGBA     kWhite = RGBA::White;

	static void FillTextToCapacity(DebugFrameData& fd)
	{
		for (uint32_t i = 0; i < DebugFrameData::kTextCapacity; ++i)
			fd.RequestDrawText(kZero, "x", 12.0f, kWhite);
	}
}

// ===========================================================================
// Suite: Submission
// ===========================================================================

TEST(DebugTextPrimitive_Submission, RequestDrawText_ProducesPrimitive)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kCyan);
	EXPECT_EQ(fd.GetTextPrimitiveCount(), 1u);
	EXPECT_EQ(fd.GetDebugPrimitiveCount(), 0u);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_PositionPreserved)
{
	DebugFrameData fd;
	const Vector2D pos(10.0f, 20.0f);
	fd.RequestDrawText(pos, "label", 14.0f, kWhite);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	const DebugPrimitiveText2D& t = fd.GetTextPrimitive(0u);
	EXPECT_FLOAT_EQ(t.position.x, 10.0f);
	EXPECT_FLOAT_EQ(t.position.y, 20.0f);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ColourPreserved)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", 12.0f, kCyan);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	const DebugPrimitiveText2D& t = fd.GetTextPrimitive(0u);
	EXPECT_EQ(t.colour, kCyan);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_FontSizePreserved)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", 14.0f, kWhite);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	EXPECT_FLOAT_EQ(fd.GetTextPrimitive(0u).fontSize, 14.0f);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ShortString)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kWhite);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	EXPECT_STREQ(fd.GetTextPrimitive(0u).text, "hello");
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_MaxLengthString)
{
	char src[64];
	for (int i = 0; i < 63; ++i)
		src[i] = 'A';
	src[63] = '\0';

	DebugFrameData fd;
	fd.RequestDrawText(kPos, src, 12.0f, kWhite);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	const char* stored = fd.GetTextPrimitive(0u).text;
	EXPECT_EQ(std::strlen(stored), 63u);
	EXPECT_EQ(stored[63], '\0');
	for (int i = 0; i < 63; ++i)
		EXPECT_EQ(stored[i], 'A') << "Mismatch at index " << i;
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_OverLengthTruncated)
{
	char src[81];
	for (int i = 0; i < 80; ++i)
		src[i] = 'B';
	src[80] = '\0';

	DebugFrameData fd;
	fd.RequestDrawText(kPos, src, 12.0f, kWhite);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	const char* stored = fd.GetTextPrimitive(0u).text;
	EXPECT_EQ(std::strlen(stored), 63u);
	EXPECT_EQ(stored[63], '\0');
	for (int i = 0; i < 63; ++i)
		EXPECT_EQ(stored[i], 'B') << "Mismatch at index " << i;
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_NullptrText)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, nullptr, 12.0f, kWhite);

	ASSERT_EQ(fd.GetTextPrimitiveCount(), 1u);
	EXPECT_EQ(fd.GetTextPrimitive(0u).text[0], '\0');
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ZeroFontSize_NoPrimitive)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", 0.0f, kWhite);
	EXPECT_EQ(fd.GetTextPrimitiveCount(), 0u);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_NegativeFontSize_NoPrimitive)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", -1.0f, kWhite);
	EXPECT_EQ(fd.GetTextPrimitiveCount(), 0u);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ObeysTextCapacityGuard)
{
	DebugFrameData fd;
	FillTextToCapacity(fd);

	EXPECT_EQ(fd.DroppedTextCount(), 0u);
	EXPECT_EQ(fd.GetTextPrimitiveCount(), DebugFrameData::kTextCapacity);

	fd.RequestDrawText(kPos, "overflow", 12.0f, kWhite);
	EXPECT_EQ(fd.DroppedTextCount(), 1u);
}

TEST(DebugTextPrimitive_Submission, GeometryAndTextBudgetsAreIndependent)
{
	// Filling text to capacity must not affect geometry capacity, and vice versa.
	DebugFrameData fd;
	FillTextToCapacity(fd);
	fd.RequestDraw(kZero, 1.0f, kWhite);

	EXPECT_EQ(fd.GetTextPrimitiveCount(),    DebugFrameData::kTextCapacity);
	EXPECT_EQ(fd.GetDebugPrimitiveCount(),   1u);
	EXPECT_EQ(fd.DroppedTextCount(),         0u);
	EXPECT_EQ(fd.DroppedCount(),             0u);
}

TEST(DebugTextPrimitive_Submission, ClearResetsTextBuffer)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kWhite);
	fd.RequestDrawText(kPos, "world", 12.0f, kWhite);
	ASSERT_EQ(fd.GetTextPrimitiveCount(), 2u);

	fd.ClearDebugBuffer();
	EXPECT_EQ(fd.GetTextPrimitiveCount(), 0u);
	EXPECT_EQ(fd.DroppedTextCount(),      0u);
}

TEST(DebugTextPrimitive_Submission, CopyPreservesTextBuffer)
{
	DebugFrameData src;
	src.RequestDrawText(kPos, "abc", 10.0f, kCyan);
	src.RequestDrawText(kZero, "xyz", 8.0f, kWhite);

	DebugFrameData dst;
	dst.CopyDebugBuffer(src);

	ASSERT_EQ(dst.GetTextPrimitiveCount(), 2u);
	EXPECT_STREQ(dst.GetTextPrimitive(0u).text, "abc");
	EXPECT_STREQ(dst.GetTextPrimitive(1u).text, "xyz");
}

TEST(DebugTextPrimitive_Submission, CopyIsIndependentFromSource)
{
	DebugFrameData src;
	src.RequestDrawText(kPos, "keep", 10.0f, kWhite);

	DebugFrameData dst;
	dst.CopyDebugBuffer(src);

	src.ClearDebugBuffer();
	EXPECT_EQ(src.GetTextPrimitiveCount(), 0u);
	EXPECT_EQ(dst.GetTextPrimitiveCount(), 1u);
}

TEST(DebugTextPrimitive_Submission, VisitorReceivesTextViaSeparatePath)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kCyan);
	fd.RequestDraw(kZero, 1.0f, kWhite);  // one geometry circle

	RecordingDebugVisitor v;
	fd.AcceptVisitor(v);

	EXPECT_EQ(v.TextCount(),   1);
	EXPECT_EQ(v.CircleCount(), 1);
	EXPECT_EQ(v.TotalCount(),  2);
}

// ===========================================================================
// Suite: Copy (DebugPrimitiveText2D direct struct)
// ===========================================================================

TEST(DebugTextPrimitive_Copy, Text2D_CopyConstruct)
{
	DebugPrimitiveText2D src;
	src.position = Vector2D(3.0f, 7.0f);
	src.fontSize = 16.0f;
	src.colour   = RGBA::Red;
	strncpy_s(src.text, sizeof(src.text), "copy test", _TRUNCATE);

	DebugPrimitiveText2D copy(src);

	EXPECT_FLOAT_EQ(copy.position.x, 3.0f);
	EXPECT_FLOAT_EQ(copy.position.y, 7.0f);
	EXPECT_FLOAT_EQ(copy.fontSize, 16.0f);
	EXPECT_EQ(copy.colour, RGBA::Red);
	EXPECT_STREQ(copy.text, "copy test");
}

TEST(DebugTextPrimitive_Copy, Text2D_AssignmentOp)
{
	DebugPrimitiveText2D src;
	src.position = Vector2D(1.0f, 2.0f);
	src.fontSize = 10.0f;
	src.colour   = RGBA::Green;
	strncpy_s(src.text, sizeof(src.text), "assign", _TRUNCATE);

	DebugPrimitiveText2D dst;
	dst = src;

	EXPECT_FLOAT_EQ(dst.position.x, 1.0f);
	EXPECT_FLOAT_EQ(dst.position.y, 2.0f);
	EXPECT_FLOAT_EQ(dst.fontSize, 10.0f);
	EXPECT_EQ(dst.colour, RGBA::Green);
	EXPECT_STREQ(dst.text, "assign");
}

TEST(DebugTextPrimitive_Copy, Text2D_StringCopied)
{
	DebugPrimitiveText2D src;
	strncpy_s(src.text, sizeof(src.text), "test", _TRUNCATE);
	src.fontSize = 12.0f;
	src.colour   = RGBA::White;
	src.position = kZero;

	DebugPrimitiveText2D copy(src);

	EXPECT_STREQ(copy.text, "test");
}
