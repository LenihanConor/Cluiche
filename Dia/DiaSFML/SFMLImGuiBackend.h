////////////////////////////////////////////////////////////////////////////////
// Filename: SFMLImGuiBackend.h
// Description: Implements IImGuiBackend for SFML 3 + legacy OpenGL2.
//              Uses imgui_impl_opengl2 for rendering. Platform input is handled
//              manually via ProcessEvent() which converts SFML 3 events to ImGui IO.
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaImGui/IImGuiBackend.h>

// Forward declare
namespace sf
{
    class RenderWindow;
    class Event;
}

namespace Dia
{
    namespace SFML
    {
        class SFMLImGuiBackend : public Dia::ImGui::IImGuiBackend
        {
        public:
            explicit SFMLImGuiBackend();

            void SetWindow(sf::RenderWindow* window);

            void Init()              override;
            void Shutdown()          override;
            void NewFrame(float dt)  override;
            void Render()            override;

            // Typed -- called directly from RenderWindow event loop, not via IImGuiBackend
            void ProcessEvent(const sf::Event& event);

        private:
            sf::RenderWindow* mWindow = nullptr;
            bool mInitialized = false;
        };

    } // namespace SFML
} // namespace Dia

#endif // DIA_DEBUG
