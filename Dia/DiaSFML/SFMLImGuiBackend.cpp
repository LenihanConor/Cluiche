////////////////////////////////////////////////////////////////////////////////
// Filename: SFMLImGuiBackend.cpp
// Description: SFML 3 + OpenGL2 backend for Dear ImGui
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/SFMLImGuiBackend.h"

#ifdef DIA_DEBUG

#include <imgui.h>
#include <backends/imgui_impl_opengl2.h>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

namespace Dia
{
    namespace SFML
    {
        // -----------------------------------------------------------------
        // SFML 3 Key -> ImGuiKey mapping
        // -----------------------------------------------------------------
        static ImGuiKey SFMLKeyToImGui(sf::Keyboard::Key key)
        {
            switch (key)
            {
            case sf::Keyboard::Key::Tab:        return ImGuiKey_Tab;
            case sf::Keyboard::Key::Left:       return ImGuiKey_LeftArrow;
            case sf::Keyboard::Key::Right:      return ImGuiKey_RightArrow;
            case sf::Keyboard::Key::Up:         return ImGuiKey_UpArrow;
            case sf::Keyboard::Key::Down:       return ImGuiKey_DownArrow;
            case sf::Keyboard::Key::PageUp:     return ImGuiKey_PageUp;
            case sf::Keyboard::Key::PageDown:   return ImGuiKey_PageDown;
            case sf::Keyboard::Key::Home:       return ImGuiKey_Home;
            case sf::Keyboard::Key::End:        return ImGuiKey_End;
            case sf::Keyboard::Key::Insert:     return ImGuiKey_Insert;
            case sf::Keyboard::Key::Delete:     return ImGuiKey_Delete;
            case sf::Keyboard::Key::Backspace:  return ImGuiKey_Backspace;
            case sf::Keyboard::Key::Space:      return ImGuiKey_Space;
            case sf::Keyboard::Key::Enter:      return ImGuiKey_Enter;
            case sf::Keyboard::Key::Escape:     return ImGuiKey_Escape;
            case sf::Keyboard::Key::Apostrophe: return ImGuiKey_Apostrophe;
            case sf::Keyboard::Key::Comma:      return ImGuiKey_Comma;
            case sf::Keyboard::Key::Hyphen:     return ImGuiKey_Minus;
            case sf::Keyboard::Key::Period:     return ImGuiKey_Period;
            case sf::Keyboard::Key::Slash:      return ImGuiKey_Slash;
            case sf::Keyboard::Key::Semicolon:  return ImGuiKey_Semicolon;
            case sf::Keyboard::Key::Equal:      return ImGuiKey_Equal;
            case sf::Keyboard::Key::LBracket:   return ImGuiKey_LeftBracket;
            case sf::Keyboard::Key::Backslash:  return ImGuiKey_Backslash;
            case sf::Keyboard::Key::RBracket:   return ImGuiKey_RightBracket;
            case sf::Keyboard::Key::Grave:      return ImGuiKey_GraveAccent;
            case sf::Keyboard::Key::LControl:   return ImGuiKey_LeftCtrl;
            case sf::Keyboard::Key::LShift:     return ImGuiKey_LeftShift;
            case sf::Keyboard::Key::LAlt:       return ImGuiKey_LeftAlt;
            case sf::Keyboard::Key::LSystem:    return ImGuiKey_LeftSuper;
            case sf::Keyboard::Key::RControl:   return ImGuiKey_RightCtrl;
            case sf::Keyboard::Key::RShift:     return ImGuiKey_RightShift;
            case sf::Keyboard::Key::RAlt:       return ImGuiKey_RightAlt;
            case sf::Keyboard::Key::RSystem:    return ImGuiKey_RightSuper;
            default: break;
            }

            // A-Z
            if (key >= sf::Keyboard::Key::A && key <= sf::Keyboard::Key::Z)
                return static_cast<ImGuiKey>(ImGuiKey_A + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::A)));

            // 0-9
            if (key >= sf::Keyboard::Key::Num0 && key <= sf::Keyboard::Key::Num9)
                return static_cast<ImGuiKey>(ImGuiKey_0 + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num0)));

            // F1-F12
            if (key >= sf::Keyboard::Key::F1 && key <= sf::Keyboard::Key::F12)
                return static_cast<ImGuiKey>(ImGuiKey_F1 + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::F1)));

            return ImGuiKey_None;
        }

        // -----------------------------------------------------------------
        // Constructor
        // -----------------------------------------------------------------
        SFMLImGuiBackend::SFMLImGuiBackend()
            : mWindow(nullptr)
            , mInitialized(false)
        {
        }

        void SFMLImGuiBackend::SetWindow(sf::RenderWindow* window)
        {
            mWindow = window;
        }

        // -----------------------------------------------------------------
        // Init: Create ImGui context and initialise OpenGL2 renderer backend
        // -----------------------------------------------------------------
        void SFMLImGuiBackend::Init()
        {
            if (mInitialized || mWindow == nullptr)
                return;

            ::ImGui::CreateContext();

            ImGuiIO& io = ::ImGui::GetIO();
            io.BackendPlatformName = "dia_sfml3";
            io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

            // Set display size from window
            sf::Vector2u size = mWindow->getSize();
            io.DisplaySize = ImVec2(static_cast<float>(size.x), static_cast<float>(size.y));

            ImGui_ImplOpenGL2_Init();
            mInitialized = true;
        }

        // -----------------------------------------------------------------
        // Shutdown
        // -----------------------------------------------------------------
        void SFMLImGuiBackend::Shutdown()
        {
            if (!mInitialized)
                return;

            ImGui_ImplOpenGL2_Shutdown();
            ::ImGui::DestroyContext();
            mInitialized = false;
        }

        // -----------------------------------------------------------------
        // NewFrame: Update IO state and start a new ImGui frame
        // -----------------------------------------------------------------
        void SFMLImGuiBackend::NewFrame(float dt)
        {
            if (!mInitialized || mWindow == nullptr)
                return;

            ImGuiIO& io = ::ImGui::GetIO();

            // Update display size (may have been resized)
            sf::Vector2u size = mWindow->getSize();
            io.DisplaySize = ImVec2(static_cast<float>(size.x), static_cast<float>(size.y));
            io.DeltaTime = (dt > 0.0f) ? dt : (1.0f / 60.0f);

            // Update mouse position
            if (mWindow->hasFocus())
            {
                sf::Vector2i mousePos = sf::Mouse::getPosition(*mWindow);
                io.AddMousePosEvent(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
            }

            ImGui_ImplOpenGL2_NewFrame();
            ::ImGui::NewFrame();
        }

        // -----------------------------------------------------------------
        // Render: Finalize ImGui frame and issue OpenGL2 draw calls
        // -----------------------------------------------------------------
        void SFMLImGuiBackend::Render()
        {
            if (!mInitialized)
                return;

            ::ImGui::Render();

            // Save GL states that SFML may have set
            mWindow->pushGLStates();
            mWindow->resetGLStates();

            ImGui_ImplOpenGL2_RenderDrawData(::ImGui::GetDrawData());

            mWindow->popGLStates();
        }

        // -----------------------------------------------------------------
        // ProcessEvent: Convert SFML 3 events to ImGui IO events
        // -----------------------------------------------------------------
        void SFMLImGuiBackend::ProcessEvent(const sf::Event& event)
        {
            if (!mInitialized)
                return;

            ImGuiIO& io = ::ImGui::GetIO();

            if (event.is<sf::Event::KeyPressed>())
            {
                const auto* e = event.getIf<sf::Event::KeyPressed>();
                ImGuiKey key = SFMLKeyToImGui(e->code);
                if (key != ImGuiKey_None)
                    io.AddKeyEvent(key, true);

                // Modifier keys
                io.AddKeyEvent(ImGuiMod_Ctrl,  e->control);
                io.AddKeyEvent(ImGuiMod_Shift, e->shift);
                io.AddKeyEvent(ImGuiMod_Alt,   e->alt);
                io.AddKeyEvent(ImGuiMod_Super, e->system);
            }
            else if (event.is<sf::Event::KeyReleased>())
            {
                const auto* e = event.getIf<sf::Event::KeyReleased>();
                ImGuiKey key = SFMLKeyToImGui(e->code);
                if (key != ImGuiKey_None)
                    io.AddKeyEvent(key, false);

                io.AddKeyEvent(ImGuiMod_Ctrl,  e->control);
                io.AddKeyEvent(ImGuiMod_Shift, e->shift);
                io.AddKeyEvent(ImGuiMod_Alt,   e->alt);
                io.AddKeyEvent(ImGuiMod_Super, e->system);
            }
            else if (event.is<sf::Event::TextEntered>())
            {
                const auto* e = event.getIf<sf::Event::TextEntered>();
                if (e->unicode > 0 && e->unicode < 0x10000)
                    io.AddInputCharacter(e->unicode);
            }
            else if (event.is<sf::Event::MouseMoved>())
            {
                const auto* e = event.getIf<sf::Event::MouseMoved>();
                io.AddMousePosEvent(static_cast<float>(e->position.x), static_cast<float>(e->position.y));
            }
            else if (event.is<sf::Event::MouseButtonPressed>())
            {
                const auto* e = event.getIf<sf::Event::MouseButtonPressed>();
                int button = -1;
                if (e->button == sf::Mouse::Button::Left)   button = 0;
                if (e->button == sf::Mouse::Button::Right)  button = 1;
                if (e->button == sf::Mouse::Button::Middle) button = 2;
                if (button >= 0)
                    io.AddMouseButtonEvent(button, true);
            }
            else if (event.is<sf::Event::MouseButtonReleased>())
            {
                const auto* e = event.getIf<sf::Event::MouseButtonReleased>();
                int button = -1;
                if (e->button == sf::Mouse::Button::Left)   button = 0;
                if (e->button == sf::Mouse::Button::Right)  button = 1;
                if (e->button == sf::Mouse::Button::Middle) button = 2;
                if (button >= 0)
                    io.AddMouseButtonEvent(button, false);
            }
            else if (event.is<sf::Event::MouseWheelScrolled>())
            {
                const auto* e = event.getIf<sf::Event::MouseWheelScrolled>();
                if (e->wheel == sf::Mouse::Wheel::Vertical)
                    io.AddMouseWheelEvent(0.0f, e->delta);
                else if (e->wheel == sf::Mouse::Wheel::Horizontal)
                    io.AddMouseWheelEvent(e->delta, 0.0f);
            }
            else if (event.is<sf::Event::Resized>())
            {
                const auto* e = event.getIf<sf::Event::Resized>();
                io.DisplaySize = ImVec2(static_cast<float>(e->size.x), static_cast<float>(e->size.y));
            }
            else if (event.is<sf::Event::FocusLost>())
            {
                io.AddFocusEvent(false);
            }
            else if (event.is<sf::Event::FocusGained>())
            {
                io.AddFocusEvent(true);
            }
        }

    } // namespace SFML
} // namespace Dia

#endif // DIA_DEBUG
