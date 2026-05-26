////////////////////////////////////////////////////////////////////////////////
// Filename: DiaVisualDebuggerConsole.cpp
// Description: Implementation of DiaVisualDebuggerConsole
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <imgui.h>

#include <cstring>

namespace Dia
{
    namespace Debug
    {
        // -----------------------------------------------------------------
        // ConsoleSink: ISink that appends to a ring buffer
        // -----------------------------------------------------------------
        class ConsoleSink : public Dia::Observation::Log::ISink
        {
        public:
            ConsoleSink(char logBuffer[][128], int& logHead, int& logCount,
                        bool& scrollToBottom, int capacity,
                        Dia::Observation::Log::LogLevel threshold)
                : mLogBuffer(logBuffer)
                , mLogHead(logHead)
                , mLogCount(logCount)
                , mScrollToBottom(scrollToBottom)
                , mCapacity(capacity)
            {
                SetLevelThreshold(threshold);
            }

            void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override
            {
                strncpy_s(mLogBuffer[mLogHead], 128, entry.message, 127);
                mLogBuffer[mLogHead][127] = '\0';
                mLogHead = (mLogHead + 1) % mCapacity;
                if (mLogCount < mCapacity)
                    ++mLogCount;
                mScrollToBottom = true;
            }

            const char* GetName() const override { return "DiaVisualDebuggerConsole"; }

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
            memset(mOutputBuffer,  0, sizeof(mOutputBuffer));
            memset(mWarningBuffer, 0, sizeof(mWarningBuffer));
            memset(mCommandBuffer, 0, sizeof(mCommandBuffer));
        }

        DiaVisualDebuggerConsole::~DiaVisualDebuggerConsole()
        {
            Detach();
        }

        // -----------------------------------------------------------------
        // Logger integration
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::Attach(Dia::Observation::Log::Logger& logger)
        {
            if (mOutputSink != nullptr)
                Detach();

            mOutputSink = new ConsoleSink(
                mOutputBuffer, mOutputHead, mOutputCount, mOutputScrollBottom,
                kLogTailCapacity, Dia::Observation::Log::LogLevel::kInfo);

            mWarningSink = new ConsoleSink(
                mWarningBuffer, mWarningHead, mWarningCount, mWarningScrollBottom,
                kLogTailCapacity, Dia::Observation::Log::LogLevel::kWarning);

            mAttachedLogger = &logger;
            logger.RegisterSink(mOutputSink);
            logger.RegisterSink(mWarningSink);
        }

        void DiaVisualDebuggerConsole::Detach()
        {
            if (mAttachedLogger != nullptr)
            {
                if (mOutputSink)
                    mAttachedLogger->UnregisterSink(mOutputSink);
                if (mWarningSink)
                    mAttachedLogger->UnregisterSink(mWarningSink);
            }
            delete mOutputSink;  mOutputSink  = nullptr;
            delete mWarningSink; mWarningSink = nullptr;
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
            if (index < 0 || index >= mOutputCount)
                return "";
            int idx = (mOutputHead - mOutputCount + index + kLogTailCapacity) % kLogTailCapacity;
            return mOutputBuffer[idx];
        }

        // -----------------------------------------------------------------
        // Render
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::Render(DebugLayerManager& manager,
                                              const Dia::Graphics::DebugFrameData& debugFrameData)
        {
            if (!mVisible)
                return;

            ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Game Debug Console", &mVisible))
            {
                ImGui::End();
                return;
            }

            RenderDomainTabs(manager, debugFrameData);
            ImGui::Separator();
            RenderCommandInput();
            ImGui::Separator();
            RenderBottomTabs();

            ImGui::End();
        }

        // -----------------------------------------------------------------
        // Domain tabs
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderDomainTabs(
            DebugLayerManager& manager,
            const Dia::Graphics::DebugFrameData& debugFrameData)
        {
            // Collect unique domain prefixes from registered layer names.
            // Domain = everything before the first '.' in the layer name.
            // e.g. "physics.shapes" -> "physics"
            char domains[kMaxDomains][32];
            int  domainCount = 0;

            const int layerCount = manager.GetLayerCount();
            for (int i = 0; i < layerCount; ++i)
            {
                const char* name = manager.GetLayerName(i).AsChar();
                if (!name) continue;

                // Extract prefix up to first '.'
                char prefix[32] = {};
                int j = 0;
                while (name[j] && name[j] != '.' && j < 31)
                {
                    prefix[j] = name[j];
                    ++j;
                }
                prefix[j] = '\0';

                // Check if already in list
                bool found = false;
                for (int d = 0; d < domainCount; ++d)
                {
                    if (strcmp(domains[d], prefix) == 0) { found = true; break; }
                }
                if (!found && domainCount < kMaxDomains)
                {
                    strncpy_s(domains[domainCount], 32, prefix, 31);
                    ++domainCount;
                }
            }

            if (ImGui::BeginTabBar("##DomainTabs"))
            {
                for (int d = 0; d < domainCount; ++d)
                {
                    if (ImGui::BeginTabItem(domains[d]))
                    {
                        RenderLayersSection(manager, domains[d]);
                        RenderStatsSection(debugFrameData);
                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
        }

        // -----------------------------------------------------------------
        // Layers section (for one domain)
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderLayersSection(
            DebugLayerManager& manager, const char* domain)
        {
            const bool open = ImGui::CollapsingHeader("Visual Debugger",
                ImGuiTreeNodeFlags_DefaultOpen);
            if (!open)
                return;

            const int layerCount = manager.GetLayerCount();
            for (int i = 0; i < layerCount; ++i)
            {
                Dia::Core::StringCRC name = manager.GetLayerName(i);
                const char* layerStr = name.AsChar();
                if (!layerStr) continue;

                // Only show layers that belong to this domain
                bool inDomain = true;
                const size_t domainLen = strlen(domain);
                if (strncmp(layerStr, domain, domainLen) != 0 ||
                    (layerStr[domainLen] != '.' && layerStr[domainLen] != '\0'))
                {
                    inDomain = false;
                }
                if (!inDomain)
                    continue;

                ImGui::PushID(i);

                bool enabled = manager.IsLayerEnabled(name);
                if (ImGui::Checkbox("##en", &enabled))
                {
                    if (enabled) manager.EnableLayer(name);
                    else         manager.DisableLayer(name);
                }
                ImGui::SameLine();

                // Layer name as collapsible header for DrawImGui controls
                if (ImGui::TreeNodeEx(layerStr, ImGuiTreeNodeFlags_None))
                {
                    IVisualDebugger* layer = manager.GetLayer(i);
                    if (layer)
                        layer->DrawImGui();
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }

        // -----------------------------------------------------------------
        // Stats section
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderStatsSection(
            const Dia::Graphics::DebugFrameData& debugFrameData)
        {
            if (!ImGui::CollapsingHeader("Stats"))
                return;

            ImGui::Text("Primitives: %u / %u",
                debugFrameData.GetDebugPrimitiveCount(),
                Dia::Graphics::DebugFrameData::kCapacity);

            if (debugFrameData.DroppedCount() > 0)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                    "DROPPED: %u", debugFrameData.DroppedCount());
            }
        }

        // -----------------------------------------------------------------
        // Command input
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderCommandInput()
        {
            ImGui::Text("Command:");
            ImGui::SameLine();
            const bool execute = ImGui::InputText("##cmd", mCommandBuffer,
                sizeof(mCommandBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
            if (execute && mCommandBuffer[0] != '\0')
            {
                const Dia::API::CommandInfo* cmd =
                    Dia::API::GetCommand(Dia::Core::StringCRC(mCommandBuffer));
                if (cmd != nullptr && cmd->callback)
                {
                    Dia::API::CommandArgs args;
                    cmd->callback(args);
                }
                mCommandBuffer[0] = '\0';
            }
        }

        // -----------------------------------------------------------------
        // Bottom tabs: Output | Warnings
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderBottomTabs()
        {
            if (!ImGui::BeginTabBar("##BottomTabs"))
                return;

            if (ImGui::BeginTabItem("Output"))
            {
                ImGui::BeginChild("OutputLog", ImVec2(0, 100), false);
                for (int i = 0; i < mOutputCount; ++i)
                {
                    int idx = (mOutputHead - mOutputCount + i + kLogTailCapacity)
                              % kLogTailCapacity;
                    ImGui::TextUnformatted(mOutputBuffer[idx]);
                }
                if (mOutputScrollBottom)
                {
                    ImGui::SetScrollHereY(1.0f);
                    mOutputScrollBottom = false;
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Warnings"))
            {
                ImGui::BeginChild("WarningLog", ImVec2(0, 100), false);
                for (int i = 0; i < mWarningCount; ++i)
                {
                    int idx = (mWarningHead - mWarningCount + i + kLogTailCapacity)
                              % kLogTailCapacity;
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                        "%s", mWarningBuffer[idx]);
                }
                if (mWarningScrollBottom)
                {
                    ImGui::SetScrollHereY(1.0f);
                    mWarningScrollBottom = false;
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
