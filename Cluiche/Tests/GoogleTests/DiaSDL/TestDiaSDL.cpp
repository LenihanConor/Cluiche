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
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_A) == Dia::Input::EKey::A);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_Z) == Dia::Input::EKey::Z);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_M) == Dia::Input::EKey::M);
}

TEST(DiaSDLEKeyMapping, NumericKeys)
{
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_0) == Dia::Input::EKey::Num0);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_9) == Dia::Input::EKey::Num9);
}

TEST(DiaSDLEKeyMapping, SpecialKeys)
{
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_ESCAPE)    == Dia::Input::EKey::Escape);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_RETURN)    == Dia::Input::EKey::Return);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_SPACE)     == Dia::Input::EKey::Space);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_BACKSPACE) == Dia::Input::EKey::BackSpace);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_TAB)       == Dia::Input::EKey::Tab);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_LEFT)      == Dia::Input::EKey::Left);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_RIGHT)     == Dia::Input::EKey::Right);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_UP)        == Dia::Input::EKey::Up);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_DOWN)      == Dia::Input::EKey::Down);
}

TEST(DiaSDLEKeyMapping, FunctionKeys)
{
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_F1)  == Dia::Input::EKey::F1);
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_F12) == Dia::Input::EKey::F12);
}

TEST(DiaSDLEKeyMapping, UnknownKeycode)
{
    EXPECT_TRUE(Dia::SDL::InputSource::SDLKeycodeToEKey(SDLK_UNKNOWN) == Dia::Input::EKey::Unknown);
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
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kKeyPressed);
    EXPECT_TRUE(out.key.AsKey() == Dia::Input::EKey::A);
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
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kKeyReleased);
}

TEST(DiaSDLEventTranslation, WindowClosed)
{
    SDL_Event sdl{};
    sdl.type = SDL_EVENT_QUIT;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kClosed);
}

TEST(DiaSDLEventTranslation, WindowResized)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_WINDOW_RESIZED;
    sdl.window.data1  = 1280;
    sdl.window.data2  = 720;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kResized);
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
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kMouseMoved);
    EXPECT_EQ(out.mouseMove.x, 100);
    EXPECT_EQ(out.mouseMove.y, 200);
}

TEST(DiaSDLEventTranslation, MouseButtonPressed)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_MOUSE_BUTTON_DOWN;
    sdl.button.button = SDL_BUTTON_LEFT;
    sdl.button.x      = 50.0f;
    sdl.button.y      = 75.0f;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kMouseButtonPressed);
}

TEST(DiaSDLEventTranslation, MouseButtonReleased)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_MOUSE_BUTTON_UP;
    sdl.button.button = SDL_BUTTON_RIGHT;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kMouseButtonReleased);
}

TEST(DiaSDLEventTranslation, JoystickButtonPressed)
{
    SDL_Event sdl{};
    sdl.type           = SDL_EVENT_JOYSTICK_BUTTON_DOWN;
    sdl.jbutton.which  = 0;
    sdl.jbutton.button = 2;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kJoystickButtonPressed);
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
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kJoystickButtonReleased);
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
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kJoystickMoved);
}

TEST(DiaSDLEventTranslation, JoystickConnected)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_JOYSTICK_ADDED;
    sdl.jdevice.which = 1;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kJoystickConnected);
}

TEST(DiaSDLEventTranslation, JoystickDisconnected)
{
    SDL_Event sdl{};
    sdl.type          = SDL_EVENT_JOYSTICK_REMOVED;
    sdl.jdevice.which = 1;
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kJoystickDisconnected);
}

TEST(DiaSDLEventTranslation, TextEntered)
{
    SDL_Event sdl{};
    sdl.type      = SDL_EVENT_TEXT_INPUT;
    sdl.text.text = "H";   // SDL3: text is const char*
    Dia::Input::Event out{};
    ASSERT_TRUE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
    EXPECT_TRUE(out.type == Dia::Input::Event::EType::kTextEntered);
    EXPECT_EQ(out.text.unicode, static_cast<unsigned int>('H'));
}

TEST(DiaSDLEventTranslation, UnknownEventReturnsFalse)
{
    SDL_Event sdl{};
    sdl.type = 0xFFFFu;  // bogus type not in the translation table
    Dia::Input::Event out{};
    EXPECT_FALSE(Dia::SDL::InputSource::TranslateSDLEvent(sdl, out));
}
