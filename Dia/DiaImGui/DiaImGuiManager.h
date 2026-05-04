////////////////////////////////////////////////////////////////////////////////
// Filename: DiaImGuiManager.h
// Description: Global ImGui lifecycle manager. Forwards Init/Shutdown/NewFrame/
//              Render to a registered IImGuiBackend. Free functions forward to
//              a process-global instance (one ImGui context per process).
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
    namespace ImGui
    {
        // Forward declaration
        class IImGuiBackend;

        class DiaImGuiManager
        {
        public:
            void SetBackend(IImGuiBackend* backend);
            IImGuiBackend* GetBackend() const;

            void Init();
            void Shutdown();
            void NewFrame(float dt);
            void Render();

        private:
            IImGuiBackend* mBackend = nullptr;
        };

        // Convenience free functions -- forward to a global DiaImGuiManager instance
        DiaImGuiManager& GetManager();
        void SetBackend(IImGuiBackend* backend);
        void Init();
        void Shutdown();
        void NewFrame(float dt);
        void Render();

    } // namespace ImGui
} // namespace Dia
