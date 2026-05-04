// TestDebugTextPrimitive.cpp - Google Test unit tests for the Text2D debug primitive
//
// Tests DebugPrimitiveText2D struct, DebugPrimitiveType::Text2D enum value,
// and DebugFrameData::RequestDrawText() submission logic.

#include <gtest/gtest.h>

#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstring>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ===========================================================================
// Helpers
// ===========================================================================

namespace
{
	static const Vector2D kZero(0.0f, 0.0f);
	static const Vector2D kPos(10.0f, 20.0f);
	static const RGBA     kCyan  = RGBA::Cyan;   // kGoal equivalent
	static const RGBA     kWhite = RGBA::White;

	// Fill DebugFrameData to exactly capacity using circles
	static void FillToCapacity(DebugFrameData& fd)
	{
		for (uint32_t i = 0; i < DebugFrameData::kCapacity; ++i)
			fd.RequestDraw(kZero, 1.0f, kWhite);
	}
}

// ===========================================================================
// Suite: Submission
// ===========================================================================

TEST(DebugTextPrimitive_Submission, RequestDrawText_ProducesPrimitive)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kCyan);
	EXPECT_EQ(fd.GetDebugPrimitiveCount(), 1u);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_TypeIsText2D)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kCyan);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	EXPECT_EQ(fd.GetDebugPrimitive(0u).type, DebugPrimitiveType::Text2D);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_PositionPreserved)
{
	DebugFrameData fd;
	const Vector2D pos(10.0f, 20.0f);
	fd.RequestDrawText(pos, "label", 14.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	const DebugPrimitiveText2D& t = fd.GetDebugPrimitive(0u).text2D;
	EXPECT_FLOAT_EQ(t.position.x, 10.0f);
	EXPECT_FLOAT_EQ(t.position.y, 20.0f);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ColourPreserved)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", 12.0f, kCyan);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	const DebugPrimitiveText2D& t = fd.GetDebugPrimitive(0u).text2D;
	EXPECT_EQ(t.colour, kCyan);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_FontSizePreserved)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", 14.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	EXPECT_FLOAT_EQ(fd.GetDebugPrimitive(0u).text2D.fontSize, 14.0f);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ShortString)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "hello", 12.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	EXPECT_STREQ(fd.GetDebugPrimitive(0u).text2D.text, "hello");
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_MaxLengthString)
{
	// 63-character string — should be stored exactly with null at [63]
	char src[64];
	for (int i = 0; i < 63; ++i)
		src[i] = 'A';
	src[63] = '\0';

	DebugFrameData fd;
	fd.RequestDrawText(kPos, src, 12.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	const char* stored = fd.GetDebugPrimitive(0u).text2D.text;
	EXPECT_EQ(std::strlen(stored), 63u);
	EXPECT_EQ(stored[63], '\0');
	// Verify content matches
	for (int i = 0; i < 63; ++i)
		EXPECT_EQ(stored[i], 'A') << "Mismatch at index " << i;
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_OverLengthTruncated)
{
	// 80-character string — should be truncated to 63 chars
	char src[81];
	for (int i = 0; i < 80; ++i)
		src[i] = 'B';
	src[80] = '\0';

	DebugFrameData fd;
	fd.RequestDrawText(kPos, src, 12.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	const char* stored = fd.GetDebugPrimitive(0u).text2D.text;
	EXPECT_EQ(std::strlen(stored), 63u);
	EXPECT_EQ(stored[63], '\0');
	// First 63 chars should all be 'B'
	for (int i = 0; i < 63; ++i)
		EXPECT_EQ(stored[i], 'B') << "Mismatch at index " << i;
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_NullptrText)
{
	// nullptr text — primitive should be stored with empty string, no crash
	DebugFrameData fd;
	fd.RequestDrawText(kPos, nullptr, 12.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	EXPECT_EQ(fd.GetDebugPrimitive(0u).text2D.text[0], '\0');
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ZeroFontSize_NoPrimitive)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", 0.0f, kWhite);
	EXPECT_EQ(fd.GetDebugPrimitiveCount(), 0u);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_NegativeFontSize_NoPrimitive)
{
	DebugFrameData fd;
	fd.RequestDrawText(kPos, "label", -1.0f, kWhite);
	EXPECT_EQ(fd.GetDebugPrimitiveCount(), 0u);
}

TEST(DebugTextPrimitive_Submission, RequestDrawText_ObeysCapacityGuard)
{
	DebugFrameData fd;
	FillToCapacity(fd);

	EXPECT_EQ(fd.DroppedCount(), 0u);

	// This text draw should be dropped
	fd.RequestDrawText(kPos, "overflow", 12.0f, kWhite);
	EXPECT_EQ(fd.DroppedCount(), 1u);
}

// ===========================================================================
// Suite: Copy
// ===========================================================================

TEST(DebugTextPrimitive_Copy, Text2D_CopyConstruct)
{
	DebugPrimitive src;
	src.type               = DebugPrimitiveType::Text2D;
	src.entityId           = 5u;
	src.text2D.position    = Vector2D(3.0f, 7.0f);
	src.text2D.fontSize    = 16.0f;
	src.text2D.colour      = RGBA::Red;
	strncpy_s(src.text2D.text, sizeof(src.text2D.text), "copy test", _TRUNCATE);
	src.text2D.text[63]    = '\0';

	DebugPrimitive copy(src);

	EXPECT_EQ(copy.type, DebugPrimitiveType::Text2D);
	EXPECT_EQ(copy.entityId, 5u);
	EXPECT_FLOAT_EQ(copy.text2D.position.x, 3.0f);
	EXPECT_FLOAT_EQ(copy.text2D.position.y, 7.0f);
	EXPECT_FLOAT_EQ(copy.text2D.fontSize, 16.0f);
	EXPECT_EQ(copy.text2D.colour, RGBA::Red);
	EXPECT_STREQ(copy.text2D.text, "copy test");
}

TEST(DebugTextPrimitive_Copy, Text2D_AssignmentOp)
{
	DebugPrimitive src;
	src.type               = DebugPrimitiveType::Text2D;
	src.entityId           = 99u;
	src.text2D.position    = Vector2D(1.0f, 2.0f);
	src.text2D.fontSize    = 10.0f;
	src.text2D.colour      = RGBA::Green;
	strncpy_s(src.text2D.text, sizeof(src.text2D.text), "assign", _TRUNCATE);

	DebugPrimitive dst;
	dst = src;

	EXPECT_EQ(dst.type, DebugPrimitiveType::Text2D);
	EXPECT_EQ(dst.entityId, 99u);
	EXPECT_FLOAT_EQ(dst.text2D.position.x, 1.0f);
	EXPECT_FLOAT_EQ(dst.text2D.position.y, 2.0f);
	EXPECT_FLOAT_EQ(dst.text2D.fontSize, 10.0f);
	EXPECT_EQ(dst.text2D.colour, RGBA::Green);
	EXPECT_STREQ(dst.text2D.text, "assign");
}

TEST(DebugTextPrimitive_Copy, Text2D_StringCopied)
{
	DebugPrimitive src;
	src.type = DebugPrimitiveType::Text2D;
	strncpy_s(src.text2D.text, sizeof(src.text2D.text), "test", _TRUNCATE);
	src.text2D.fontSize  = 12.0f;
	src.text2D.colour    = RGBA::White;
	src.text2D.position  = kZero;

	DebugPrimitive copy(src);

	EXPECT_STREQ(copy.text2D.text, "test");
}
