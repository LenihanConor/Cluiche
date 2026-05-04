////////////////////////////////////////////////////////////////////////////////
// Filename: DiaVisualDebuggerConsole.h
// Description: ImGui overlay console for DiaVisualDebugger. Provides:
//              - Checkbox tree to toggle debug layers
//              - Metrics bar (primitive count + dropped count)
//              - DiaAPI command input field
//              - Log tail (last 64 DiaLogger lines)
//              No DiaInput dependency -- caller invokes Toggle().
//              No DiaSFML dependency -- uses DiaImGui for ImGui access.
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

namespace Dia
{
    namespace Debug
    {
        class DebugLayerManager;
    }
    namespace Graphics
    {
        class DebugFrameData;
    }
    namespace Logger
    {
        class Logger;
        class ISink;
    }
}

namespace Dia
{
    namespace Debug
    {
        class DiaVisualDebuggerConsole
        {
        public:
            static constexpr int kLogTailCapacity = 64;

            DiaVisualDebuggerConsole();
            ~DiaVisualDebuggerConsole();

            void Attach(Dia::Logger::Logger& logger);
            void Detach();

            void Toggle();
            bool IsVisible() const;

            // Call each frame from the render thread, after DiaImGui::NewFrame() has run.
            void Render(DebugLayerManager& manager,
                        const Dia::Graphics::DebugFrameData& debugFrameData);

            // ----- Test-only accessors -----
            int  GetLogCount() const { return mLogCount; }
            const char* GetLogLine(int index) const;

        private:
            void RenderLayerTree(DebugLayerManager& manager);
            void RenderMetricsBar(const Dia::Graphics::DebugFrameData& debugFrameData);
            void RenderCommandInput();
            void RenderLogTail();

            bool mVisible = false;

            char mLogBuffer[kLogTailCapacity][128];
            int  mLogHead          = 0;
            int  mLogCount         = 0;
            bool mScrollToBottom   = false;

            char mCommandBuffer[256];

            Dia::Logger::ISink* mSink = nullptr;
            Dia::Logger::Logger* mAttachedLogger = nullptr;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
