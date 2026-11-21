#include <gtest/gtest.h>
#include <Modules/TestStages/CellLayersPointerFrame.h>

#include <algorithm>
#include <set>
#include <vector>

using namespace CluicheTest;
using Dia::Input::EKey;

namespace
{
	struct FakeInput
	{
		int x = 0;
		int y = 0;
		std::set<int> buttons;
		std::vector<EKey> keysDown;
		std::vector<EKey> keysPressed;

		int  GetMouseX() const { return x; }
		int  GetMouseY() const { return y; }
		bool IsMouseButtonDown(int b) const { return buttons.count(b) != 0; }
		bool IsKeyDown(EKey k) const { return std::find(keysDown.begin(), keysDown.end(), k) != keysDown.end(); }
		bool WasKeyPressed(EKey k) const { return std::find(keysPressed.begin(), keysPressed.end(), k) != keysPressed.end(); }
	};
}

TEST(CellLayersPointerFrameTests, DefaultsAreValidAndIdle)
{
	FakeInput in;
	const auto f = BuildPointerFrame(in);
	EXPECT_TRUE(f.valid);
	EXPECT_TRUE(f.hasKeyboard);
	EXPECT_FALSE(f.left || f.middle || f.right || f.shift || f.ctrl || f.alt || f.escPressed);
}

TEST(CellLayersPointerFrameTests, PositionIsPassedThrough)
{
	FakeInput in;
	in.x = 123;
	in.y = -7;
	const auto f = BuildPointerFrame(in);
	EXPECT_FLOAT_EQ(f.x, 123.0f);
	EXPECT_FLOAT_EQ(f.y, -7.0f);
}

TEST(CellLayersPointerFrameTests, ButtonIndicesMapLeftRightMiddle)
{
	FakeInput in;
	in.buttons = { 0 };
	auto f = BuildPointerFrame(in);
	EXPECT_TRUE(f.left);
	EXPECT_FALSE(f.right);
	EXPECT_FALSE(f.middle);

	in.buttons = { 1 };
	f = BuildPointerFrame(in);
	EXPECT_TRUE(f.right);
	EXPECT_FALSE(f.left);
	EXPECT_FALSE(f.middle);

	in.buttons = { 2 };
	f = BuildPointerFrame(in);
	EXPECT_TRUE(f.middle);
	EXPECT_FALSE(f.left);
	EXPECT_FALSE(f.right);
}

TEST(CellLayersPointerFrameTests, ModifiersAcceptEitherSide)
{
	FakeInput in;
	in.keysDown = { EKey::RShift, EKey::LControl, EKey::RAlt };
	auto f = BuildPointerFrame(in);
	EXPECT_TRUE(f.shift);
	EXPECT_TRUE(f.ctrl);
	EXPECT_TRUE(f.alt);

	in.keysDown = { EKey::LShift, EKey::RControl, EKey::LAlt };
	f = BuildPointerFrame(in);
	EXPECT_TRUE(f.shift);
	EXPECT_TRUE(f.ctrl);
	EXPECT_TRUE(f.alt);
}

TEST(CellLayersPointerFrameTests, EscUsesPressedEdgeNotHeld)
{
	FakeInput in;
	in.keysDown = { EKey::Escape };
	EXPECT_FALSE(BuildPointerFrame(in).escPressed);
	in.keysPressed = { EKey::Escape };
	EXPECT_TRUE(BuildPointerFrame(in).escPressed);
}
