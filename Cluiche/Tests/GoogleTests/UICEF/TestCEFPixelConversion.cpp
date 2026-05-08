////////////////////////////////////////////////////////////////////////////////
// Tests for CEFUtils BGRA-to-RGBA pixel conversion
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaUICEF/CEFUtils.h>

using namespace Dia::UICEF::Utils;

TEST(CEFPixelConversion, SinglePixel_SwapsRedAndBlue)
{
	// BGRA input: B=10, G=20, R=30, A=255
	unsigned char buffer[4] = { 10, 20, 30, 255 };
	ConvertBGRAtoRGBA(buffer, 1, 1, 0, 0, 1, 1);

	EXPECT_EQ(30, buffer[0]);  // R (was B)
	EXPECT_EQ(20, buffer[1]);  // G (unchanged)
	EXPECT_EQ(10, buffer[2]);  // B (was R)
	EXPECT_EQ(255, buffer[3]); // A (unchanged)
}

TEST(CEFPixelConversion, TransparentPixel_PreservesAlpha)
{
	unsigned char buffer[4] = { 0, 0, 0, 0 };
	ConvertBGRAtoRGBA(buffer, 1, 1, 0, 0, 1, 1);

	EXPECT_EQ(0, buffer[0]);
	EXPECT_EQ(0, buffer[1]);
	EXPECT_EQ(0, buffer[2]);
	EXPECT_EQ(0, buffer[3]);
}

TEST(CEFPixelConversion, MultiplePixels_ConvertsAll)
{
	// 2x1 image: two pixels
	unsigned char buffer[8] = {
		10, 20, 30, 255,   // Pixel 0: BGRA
		40, 50, 60, 128    // Pixel 1: BGRA
	};
	ConvertBGRAtoRGBA(buffer, 2, 1, 0, 0, 2, 1);

	EXPECT_EQ(30, buffer[0]); // Pixel 0: R
	EXPECT_EQ(20, buffer[1]); // Pixel 0: G
	EXPECT_EQ(10, buffer[2]); // Pixel 0: B
	EXPECT_EQ(255, buffer[3]);

	EXPECT_EQ(60, buffer[4]); // Pixel 1: R
	EXPECT_EQ(50, buffer[5]); // Pixel 1: G
	EXPECT_EQ(40, buffer[6]); // Pixel 1: B
	EXPECT_EQ(128, buffer[7]);
}

TEST(CEFPixelConversion, DirtyRect_OnlyConvertsRegion)
{
	// 2x2 image, only convert pixel at (1,0)
	unsigned char buffer[16] = {
		10, 20, 30, 255,   // (0,0) BGRA
		40, 50, 60, 255,   // (1,0) BGRA
		70, 80, 90, 255,   // (0,1) BGRA
		100, 110, 120, 255 // (1,1) BGRA
	};
	ConvertBGRAtoRGBA(buffer, 2, 2, 1, 0, 1, 1);

	// (0,0) unchanged
	EXPECT_EQ(10, buffer[0]);
	EXPECT_EQ(30, buffer[2]);

	// (1,0) converted
	EXPECT_EQ(60, buffer[4]); // R (was B position)
	EXPECT_EQ(40, buffer[6]); // B (was R position)

	// (0,1) unchanged
	EXPECT_EQ(70, buffer[8]);
	EXPECT_EQ(90, buffer[10]);
}
