////////////////////////////////////////////////////////////////////////////////
// Filename: DiaVisualDebuggerConsole.h
// Description: ImGui overlay console for DiaVisualDebugger. Provides:
//              - Domain tabs (one per layer name prefix, e.g. "physics", "rig")
//                  - Collapsible Draw Layers section with enable/disable + DrawImGui() per layer
//                  - Collapsible Stats section (primitive count, dropped count)
//              - Global DiaAPI command input below tabs
//              - Output / Warnings bottom tabs (ring-buffered log tail)
//              No DiaInput dependency -- caller invokes Toggle().
//              No DiaSFML dependency -- uses DiaImGui for ImGui access.
// Feature spec: docs/specs/features/cluichetest/teststages/visual-debugger-module.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

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
    namespace Observation { namespace Log
    {
        class Logger;
        class ISink;
    } }
}

namespace Dia
{
    namespace Debug
    {
        class DiaVisualDebuggerConsole
        {
        public:
            static constexpr int kLogTailCapacity = 64;
            static constexpr int kMaxDomains      = 16;

            DiaVisualDebuggerConsole();
            ~DiaVisualDebuggerConsole();

            void Attach(Dia::Observation::Log::Logger& logger);
            void Detach();

            void Toggle();
            bool IsVisible() const;

            // Call each frame from the render thread, after DiaImGui::NewFrame() has run.
            // currentStageId: the stage currently active in the application (used to auto-select tab).
            void Render(DebugLayerManager& manager,
                        const Dia::Graphics::DebugFrameData& debugFrameData,
                        const Dia::Core::StringCRC& currentStageId);

            // ----- Test-only accessors -----
            int  GetLogCount() const { return mWarningCount; }
            const char* GetLogLine(int index) const;

        private:
            void RenderStageTabs(DebugLayerManager& manager,
                                 const Dia::Core::StringCRC& currentStageId);
            void RenderLayerList(DebugLayerManager& manager,
                                 const Dia::Core::StringCRC& stageTag,
                                 bool enabled);
            void RenderCommandInput();
            void RenderBottomTabs();

            bool mVisible = false;

            // Output tab (all log levels)
            char mOutputBuffer[kLogTailCapacity][128];
            int  mOutputHead        = 0;
            int  mOutputCount       = 0;
            bool mOutputScrollBottom = false;

            // Warnings tab (warnings + errors only)
            char mWarningBuffer[kLogTailCapacity][128];
            int  mWarningHead        = 0;
            int  mWarningCount       = 0;
            bool mWarningScrollBottom = false;

            char mCommandBuffer[256];

            Dia::Observation::Log::ISink*  mOutputSink  = nullptr;
            Dia::Observation::Log::ISink*  mWarningSink = nullptr;
            Dia::Observation::Log::Logger* mAttachedLogger = nullptr;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
