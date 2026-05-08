////////////////////////////////////////////////////////////////////////////////
// Filename: DiaVisualDebuggerConsole.cpp
// Description: Implementation of DiaVisualDebuggerConsole
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogEntry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <imgui.h>

#include <cstring>

namespace Dia
{
    namespace Debug
    {
        // -----------------------------------------------------------------
        // ConsoleSink: ISink implementation that writes to the ring buffer
        // -----------------------------------------------------------------
        class ConsoleSink : public Dia::Logger::ISink
        {
        public:
            ConsoleSink(char logBuffer[][128], int& logHead, int& logCount, bool& scrollToBottom, int capacity)
                : mLogBuffer(logBuffer)
                , mLogHead(logHead)
                , mLogCount(logCount)
                , mScrollToBottom(scrollToBottom)
                , mCapacity(capacity)
            {
                // Accept warnings and errors by default
                SetLevelThreshold(Dia::Logger::LogLevel::kWarning);
            }

            void OnLogEntry(const Dia::Logger::LogEntry& entry) override
            {
                // Copy message into ring buffer
                strncpy_s(mLogBuffer[mLogHead], 128, entry.message, 127);
                mLogBuffer[mLogHead][127] = '\0';

                mLogHead = (mLogHead + 1) % mCapacity;
                if (mLogCount < mCapacity)
                    ++mLogCount;

                mScrollToBottom = true;
            }

            const char* GetName() const override
            {
                return "DiaVisualDebuggerConsole";
            }

        private:
            char (*mLogBuffer)[128];
            int& mLogHead;
            int& mLogCount;
            bool& mScrollToBottom;
            int mCapacity;
        };

        // -----------------------------------------------------------------
        // Constructor / Destructor
        // -----------------------------------------------------------------

        DiaVisualDebuggerConsole::DiaVisualDebuggerConsole()
        {
            memset(mLogBuffer, 0, sizeof(mLogBuffer));
            memset(mCommandBuffer, 0, sizeof(mCommandBuffer));
        }

        DiaVisualDebuggerConsole::~DiaVisualDebuggerConsole()
        {
            Detach();
        }

        // -----------------------------------------------------------------
        // Logger integration
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::Attach(Dia::Logger::Logger& logger)
        {
            if (mSink != nullptr)
                Detach();

            mSink = new ConsoleSink(mLogBuffer, mLogHead, mLogCount, mScrollToBottom, kLogTailCapacity);
            mAttachedLogger = &logger;
            logger.RegisterSink(mSink);
        }

        void DiaVisualDebuggerConsole::Detach()
        {
            if (mAttachedLogger != nullptr && mSink != nullptr)
            {
                mAttachedLogger->UnregisterSink(mSink);
            }
            if (mSink != nullptr)
            {
                delete mSink;
                mSink = nullptr;
            }
            mAttachedLogger = nullptr;
        }

        // -----------------------------------------------------------------
        // Toggle / Visibility
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::Toggle()
        {
            mVisible = !mVisible;
        }

        bool DiaVisualDebuggerConsole::IsVisible() const
        {
            return mVisible;
        }

        // -----------------------------------------------------------------
        // Test accessor
        // -----------------------------------------------------------------

        const char* DiaVisualDebuggerConsole::GetLogLine(int index) const
        {
            if (index < 0 || index >= mLogCount)
                return "";
            int idx = (mLogHead - mLogCount + index + kLogTailCapacity) % kLogTailCapacity;
            return mLogBuffer[idx];
        }

        // -----------------------------------------------------------------
        // Render
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::Render(DebugLayerManager& manager,
                                              const Dia::Graphics::DebugFrameData& debugFrameData)
        {
            if (!mVisible)
                return;

            ::ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            if (!::ImGui::Begin("Debug Console", &mVisible))
            {
                ::ImGui::End();
                return;
            }

            RenderLayerTree(manager);
            ::ImGui::Separator();
            RenderMetricsBar(debugFrameData);
            ::ImGui::Separator();
            RenderCommandInput();
            ::ImGui::Separator();
            RenderLogTail();

            ::ImGui::End();
        }

        // -----------------------------------------------------------------
        // Layer tree
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderLayerTree(DebugLayerManager& manager)
        {
            ::ImGui::Text("Debug Layers");
            ::ImGui::Separator();
            for (int i = 0; i < manager.GetLayerCount(); ++i)
            {
                Dia::Core::StringCRC name = manager.GetLayerName(i);
                bool enabled = manager.IsLayerEnabled(name);
                if (::ImGui::Checkbox(name.AsChar(), &enabled))
                {
                    if (enabled) manager.EnableLayer(name);
                    else         manager.DisableLayer(name);
                }
            }
        }

        // -----------------------------------------------------------------
        // Metrics bar
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderMetricsBar(const Dia::Graphics::DebugFrameData& debugFrameData)
        {
            ::ImGui::Text("Primitives: %u / %u",
                debugFrameData.GetDebugPrimitiveCount(),
                Dia::Graphics::DebugFrameData::kCapacity);

            if (debugFrameData.DroppedCount() > 0)
            {
                ::ImGui::SameLine();
                ::ImGui::TextColored(ImVec4(1, 0, 0, 1), "DROPPED: %u", debugFrameData.DroppedCount());
            }
        }

        // -----------------------------------------------------------------
        // Command input
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderCommandInput()
        {
            ::ImGui::Text("Command:");
            ::ImGui::SameLine();
            bool execute = ::ImGui::InputText("##cmd", mCommandBuffer, sizeof(mCommandBuffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
            if (execute && mCommandBuffer[0] != '\0')
            {
                // Look up command and execute
                const Dia::API::CommandInfo* cmd = Dia::API::GetCommand(Dia::Core::StringCRC(mCommandBuffer));
                if (cmd != nullptr && cmd->callback)
                {
                    Dia::API::CommandArgs args;
                    cmd->callback(args);
                }
                mCommandBuffer[0] = '\0';
            }
        }

        // -----------------------------------------------------------------
        // Log tail
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderLogTail()
        {
            ::ImGui::BeginChild("LogTail", ImVec2(0, 120), false);
            for (int i = 0; i < mLogCount; ++i)
            {
                int idx = (mLogHead - mLogCount + i + kLogTailCapacity) % kLogTailCapacity;
                ::ImGui::TextUnformatted(mLogBuffer[idx]);
            }
            if (mScrollToBottom)
            {
                ::ImGui::SetScrollHereY(1.0f);
                mScrollToBottom = false;
            }
            ::ImGui::EndChild();
        }

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
