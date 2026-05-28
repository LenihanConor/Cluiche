////////////////////////////////////////////////////////////////////////////////
// Filename: DiaVisualDebuggerConsole.cpp
// Description: Implementation of DiaVisualDebuggerConsole
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugLayerManager.h>
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
            if (index < 0 || index >= mWarningCount)
                return "";
            int idx = (mWarningHead - mWarningCount + index + kLogTailCapacity) % kLogTailCapacity;
            return mWarningBuffer[idx];
        }

        // -----------------------------------------------------------------
        // Render
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::Render(DebugLayerManager& manager,
                                              const Dia::Graphics::DebugFrameData& /*debugFrameData*/,
                                              const Dia::Core::StringCRC& currentStageId)
        {
            if (!mVisible)
                return;

            ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Game Debug Console", &mVisible))
            {
                ImGui::End();
                return;
            }

            RenderStageTabs(manager, currentStageId);
            ImGui::Separator();
            RenderCommandInput();
            ImGui::Separator();
            RenderBottomTabs();

            ImGui::End();
        }

        // -----------------------------------------------------------------
        // Stage tabs
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderStageTabs(
            DebugLayerManager& manager,
            const Dia::Core::StringCRC& currentStageId)
        {
            // Collect unique stage tags from registered layers.
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> stageTags;
            manager.GetStageTags(stageTags);

            if (ImGui::BeginTabBar("##StageTabs"))
            {
                // "Global" tab for layers with no stage tag
                if (ImGui::BeginTabItem("Global"))
                {
                    RenderLayerList(manager, Dia::Core::StringCRC(), true /*alwaysActive*/);
                    ImGui::EndTabItem();
                }

                // One tab per stage that has registered layers
                for (unsigned int i = 0; i < stageTags.Size(); ++i)
                {
                    const Dia::Core::StringCRC& tag = stageTags[i];
                    bool isActive  = manager.IsStageActive(tag);
                    bool isCurrent = (tag == currentStageId);

                    // Gray label for inactive stages
                    if (!isActive)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

                    bool open = ImGui::BeginTabItem(
                        tag.AsChar(),
                        nullptr,
                        isCurrent ? ImGuiTabItemFlags_SetSelected : 0);

                    if (!isActive)
                        ImGui::PopStyleColor();

                    if (open)
                    {
                        if (!isActive)
                            ImGui::TextDisabled("(stage not active)");
                        RenderLayerList(manager, tag, isActive);
                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
        }

        // -----------------------------------------------------------------
        // Layer list (for one stage tag)
        // -----------------------------------------------------------------

        void DiaVisualDebuggerConsole::RenderLayerList(
            DebugLayerManager& manager,
            const Dia::Core::StringCRC& stageTag,
            bool enabled)
        {
            const int layerCount = manager.GetLayerCount();
            for (int i = 0; i < layerCount; ++i)
            {
                // Only show layers whose stageTag matches the requested tag
                if (manager.GetLayerStageTag(i) != stageTag)
                    continue;

                Dia::Core::StringCRC name = manager.GetLayerName(i);
                const char* layerStr = name.AsChar();
                if (!layerStr || layerStr[0] == '\0')
                    continue;

                ImGui::PushID(i);

                bool layerEnabled = manager.IsLayerEnabled(name);
                if (!enabled)
                {
                    // Stage is inactive — show grayed, non-interactive
                    ImGui::BeginDisabled(true);
                    ImGui::Checkbox("##en", &layerEnabled);
                    ImGui::EndDisabled();
                }
                else
                {
                    if (ImGui::Checkbox("##en", &layerEnabled))
                    {
                        if (layerEnabled) manager.EnableLayer(name);
                        else              manager.DisableLayer(name);
                    }
                }
                ImGui::SameLine();
                ImGui::Text("%s", layerStr);

                ImGui::PopID();
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
