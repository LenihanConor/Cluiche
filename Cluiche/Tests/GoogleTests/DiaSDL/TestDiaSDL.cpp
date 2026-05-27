////////////////////////////////////////////////////////////////////////////////
// Filename: TestDiaSDL.cpp
// RED tests for DiaSDL::InputSource — EKey lookup table and AC-09 event
// translations using synthetic SDL_Events.
//
// These tests WILL NOT COMPILE until T-06 implements DiaSDL::InputSource.
// That is the correct TDD RED state.  They go GREEN when T-06 fills the
// lookup table and TranslateSDLEvent implementation.
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"

// The header being tested — does not exist until T-06 (RED by design)
#include <DiaSDL/InputSource.h>

#include <DiaInput/Event.h>
#include <DiaInput/EKey.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

////////////////////////////////////////////////////////////////////////////////
// Local helper
////////////////////////////////////////////////////////////////////////////////
namespace
{
    static SDL_Event MakeKeyEvent(Uint32 type, SDL_Keycode key,
                                  bool shift = false,
                                  bool ctrl  = false,
                                  bool alt   = false)
    {
        SDL_Event e{};
        e.type     = type;
        e.key.key  = key;
        e.key.mod  = 0;
        if (shift) { e.key.mod |= SDL_KMOD_SHIFT; }
        if (ctrl)  { e.key.mod |= SDL_KMOD_CTRL;  }
        if (alt)   { e.key.mod |= SDL_KMOD_ALT;   }
        return e;
    }
} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// Suite 1: EKey lookup table — SDL_Keycode → Dia::Input::EKey round-trips
////////////////////////////////////////////////////////////////////////////////

TEST(DiaSDLEKeyMapping, AlphaKeys)
{
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_A).AsInt(), Dia::Input::EKey::A.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_Z).AsInt(), Dia::Input::EKey::Z.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_M).AsInt(), Dia::Input::EKey::M.AsInt());
}

TEST(DiaSDLEKeyMapping, NumericKeys)
{
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_0).AsInt(), Dia::Input::EKey::Num0.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_9).AsInt(), Dia::Input::EKey::Num9.AsInt());
}

TEST(DiaSDLEKeyMapping, SpecialKeys)
{
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_ESCAPE).AsInt(),    Dia::Input::EKey::Escape.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_RETURN).AsInt(),    Dia::Input::EKey::Return.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_SPACE).AsInt(),     Dia::Input::EKey::Space.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_BACKSPACE).AsInt(), Dia::Input::EKey::BackSpace.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_TAB).AsInt(),       Dia::Input::EKey::Tab.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_LEFT).AsInt(),      Dia::Input::EKey::Left.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_RIGHT).AsInt(),     Dia::Input::EKey::Right.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_UP).AsInt(),        Dia::Input::EKey::Up.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_DOWN).AsInt(),      Dia::Input::EKey::Down.AsInt());
}

TEST(DiaSDLEKeyMapping, FunctionKeys)
{
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_F1).AsInt(),  Dia::Input::EKey::F1.AsInt());
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_F12).AsInt(), Dia::Input::EKey::F12.AsInt());
}

TEST(DiaSDLEKeyMapping, UnknownKeycode)
{
    // An unmapped keycode must return EKey::Unknown
    EXPECT_EQ(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_UNKNOWN).AsInt(),
              Dia::Input::EKey::Unknown.AsInt());
}

////////////////////////////////////////////////////////////////////////////////
// Suite 2: SDL event → Dia::Input::Event translation
//
// TranslateSDLEvent(const SDL_Event&, Dia::Input::Event&) returns true when
// the SDL event type is recognised and translated, false when skipped.
////////////////////////////////////////////////////////////////////////////////

TEST(DiaSDLEventTranslation, KeyPressed)
{
    SDL_Event sdl = MakeKeyEvent(SDL_EVENT_KEY_DOWN, SDLK_A);
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kKeyPressed.AsInt());
    EXPECT_EQ(out.key.AsKey().AsInt(), Dia::Input::EKey::A.AsInt());
    EXPECT_FALSE(out.key.shift);
}

TEST(DiaSDLEventTranslation, KeyPressedWithModifiers)
{
    SDL_Event sdl = MakeKeyEvent(SDL_EVENT_KEY_DOWN, SDLK_S, /*shift=*/true, /*ctrl=*/true, /*alt=*/false);
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.key.shift);
    EXPECT_TRUE(out.key.control);
    EXPECT_FALSE(out.key.alt);
}

TEST(DiaSDLEventTranslation, KeyReleased)
{
    SDL_Event sdl = MakeKeyEvent(SDL_EVENT_KEY_UP, SDLK_ESCAPE);
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kKeyReleased.AsInt());
}

TEST(DiaSDLEventTranslation, WindowClosed)
{
    SDL_Event sdl{};
    sdl.type = SDL_EVENT_QUIT;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kClosed.AsInt());
}

TEST(DiaSDLEventTranslation, WindowResized)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_WINDOW_RESIZED;
    sdl.window.data1  = 1280;
    sdl.window.data2  = 720;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kResized.AsInt());
    EXPECT_EQ(out.size.width,  1280u);
    EXPECT_EQ(out.size.height, 720u);
}

TEST(DiaSDLEventTranslation, MouseMoved)
{
    SDL_Event sdl{};
    sdl.type      = SDL_EVENT_MOUSE_MOTION;
    sdl.motion.x  = 100.0f;
    sdl.motion.y  = 200.0f;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kMouseMoved.AsInt());
    EXPECT_EQ(out.mouseMove.x, 100);
    EXPECT_EQ(out.mouseMove.y, 200);
}

TEST(DiaSDLEventTranslation, MouseButtonPressed)
{
    SDL_Event sdl{};
    sdl.type         = SDL_EVENT_MOUSE_BUTTON_DOWN;
    sdl.button.button = SDL_BUTTON_LEFT;
    sdl.button.x     = 50.0f;
    sdl.button.y     = 75.0f;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kMouseButtonPressed.AsInt());
}

TEST(DiaSDLEventTranslation, MouseButtonReleased)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_MOUSE_BUTTON_UP;
    sdl.button.button = SDL_BUTTON_RIGHT;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kMouseButtonReleased.AsInt());
}

TEST(DiaSDLEventTranslation, JoystickButtonPressed)
{
    SDL_Event sdl{};
    sdl.type           = SDL_EVENT_JOYSTICK_BUTTON_DOWN;
    sdl.jbutton.which  = 0;
    sdl.jbutton.button = 2;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kJoystickButtonPressed.AsInt());
    EXPECT_EQ(out.joystickButton.button, 2u);
}

TEST(DiaSDLEventTranslation, JoystickButtonReleased)
{
    SDL_Event sdl{};
    sdl.type           = SDL_EVENT_JOYSTICK_BUTTON_UP;
    sdl.jbutton.which  = 0;
    sdl.jbutton.button = 3;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kJoystickButtonReleased.AsInt());
}

TEST(DiaSDLEventTranslation, JoystickAxisMoved)
{
    SDL_Event sdl{};
    sdl.type        = SDL_EVENT_JOYSTICK_AXIS_MOTION;
    sdl.jaxis.which = 0;
    sdl.jaxis.axis  = 1;
    sdl.jaxis.value = 16000;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kJoystickMoved.AsInt());
}

TEST(DiaSDLEventTranslation, JoystickConnected)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_JOYSTICK_ADDED;
    sdl.jdevice.which = 1;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kJoystickConnected.AsInt());
}

TEST(DiaSDLEventTranslation, JoystickDisconnected)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_JOYSTICK_REMOVED;
    sdl.jdevice.which = 1;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kJoystickDisconnected.AsInt());
}

TEST(DiaSDLEventTranslation, TextEntered)
{
    SDL_Event sdl{};
    sdl.type         = SDL_EVENT_TEXT_INPUT;
    sdl.text.text[0] = 'H';
    sdl.text.text[1] = '\0';
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_EQ(out.type.AsInt(), Dia::Input::Event::EType::kTextEntered.AsInt());
}

TEST(DiaSDLEventTranslation, UnknownEventReturnsFalse)
{
    SDL_Event sdl{};
    sdl.type = 0xFFFFu;  // bogus type not in the translation table
    Dia::Input::Event out{};
    EXPECT_FALSE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
}
