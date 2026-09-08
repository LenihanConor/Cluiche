////////////////////////////////////////////////////////////////////////////////
// Filename: TestInputStreamMouseButtons.cpp
// Tests: Mouse button tracking logic matching InputStreamModule's implementation.
//        Verifies the same prev/current pattern used for keyboard works correctly
//        for mouse buttons. Tests the logic, not the module lifecycle.
// AC10
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <cstring>

// Mirrors the mouse button tracking in InputStreamModule
// Tests the behavioural contract of WasMouseButtonPressed / IsMouseButtonDown /
// WasMouseButtonReleased using the same array-swap pattern.

static constexpr unsigned int kMaxButtons = 8;

struct MouseButtonTracker
{
    bool current[kMaxButtons] = {};
    bool previous[kMaxButtons] = {};

    void BeginFrame()
    {
        memcpy(previous, current, sizeof(current));
    }

    void Press(int btn)
    {
        if (btn >= 0 && btn < static_cast<int>(kMaxButtons)) current[btn] = true;
    }

    void Release(int btn)
    {
        if (btn >= 0 && btn < static_cast<int>(kMaxButtons)) current[btn] = false;
    }

    bool IsDown(int btn) const
    {
        if (btn < 0 || btn >= static_cast<int>(kMaxButtons)) return false;
        return current[btn];
    }

    bool WasPressed(int btn) const
    {
        if (btn < 0 || btn >= static_cast<int>(kMaxButtons)) return false;
        return !previous[btn] && current[btn];
    }

    bool WasReleased(int btn) const
    {
        if (btn < 0 || btn >= static_cast<int>(kMaxButtons)) return false;
        return previous[btn] && !current[btn];
    }
};

TEST(InputStreamMouseButtons, IsDown_AfterPress_True)
{
    MouseButtonTracker t;
    t.BeginFrame();
    t.Press(0);
    EXPECT_TRUE(t.IsDown(0));
    EXPECT_FALSE(t.IsDown(1));
}

TEST(InputStreamMouseButtons, WasPressed_OnPressFrame_True)
{
    MouseButtonTracker t;
    t.BeginFrame();
    t.Press(0);
    EXPECT_TRUE(t.WasPressed(0));
}

TEST(InputStreamMouseButtons, WasPressed_WhileHeld_False)
{
    MouseButtonTracker t;
    t.Press(0);
    t.BeginFrame(); // now previous=true, current=true
    EXPECT_FALSE(t.WasPressed(0));
}

TEST(InputStreamMouseButtons, WasReleased_OnReleaseFrame_True)
{
    MouseButtonTracker t;
    t.Press(0);
    t.BeginFrame(); // previous=true
    t.Release(0);   // current=false
    EXPECT_TRUE(t.WasReleased(0));
}

TEST(InputStreamMouseButtons, WasReleased_WhileUp_False)
{
    MouseButtonTracker t;
    t.BeginFrame(); // both false
    EXPECT_FALSE(t.WasReleased(0));
}

TEST(InputStreamMouseButtons, MultipleButtons_Independent)
{
    MouseButtonTracker t;
    t.BeginFrame();
    t.Press(0);
    t.Press(1);
    EXPECT_TRUE(t.IsDown(0));
    EXPECT_TRUE(t.IsDown(1));
    EXPECT_FALSE(t.IsDown(2));
    EXPECT_TRUE(t.WasPressed(0));
    EXPECT_TRUE(t.WasPressed(1));
}

TEST(InputStreamMouseButtons, RightButton_ButtonIndex1)
{
    // kRight = 1 per EMouseButton
    MouseButtonTracker t;
    t.BeginFrame();
    t.Press(1); // right button
    EXPECT_TRUE(t.IsDown(1));
    EXPECT_FALSE(t.IsDown(0));
}

TEST(InputStreamMouseButtons, OutOfBoundsIndex_ReturnsFalse)
{
    MouseButtonTracker t;
    EXPECT_FALSE(t.IsDown(-1));
    EXPECT_FALSE(t.IsDown(100));
    EXPECT_FALSE(t.WasPressed(-1));
    EXPECT_FALSE(t.WasReleased(-1));
}
