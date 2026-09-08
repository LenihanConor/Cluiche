////////////////////////////////////////////////////////////////////////////////
// Filename: BlackboardVisualDebugger.cpp
// Description: IDebugDomain implementation for DiaBlackboard.
// System spec: docs/specs/applications/dia/systems/diablackboardvisualdebugger/diablackboardvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include "BlackboardVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaBlackboard/Blackboard.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

#include <cstdio>
#include <cstring>

namespace Dia
{
    namespace Blackboard
    {

        namespace
        {
            const Dia::Core::StringCRC kCmdToggle("toggle");
            const Dia::Core::StringCRC kDrawerSlotTable("SlotTable");

            // Format a typeTag pointer as a hex string: "0x<address>"
            void FormatTypeTag(const void* typeTag, char* buf, int bufSize)
            {
                // Use %p for pointer, which is platform-specific but acceptable here.
                // We prefix with "0x" explicitly to be consistent across platforms.
                const uintptr_t addr = reinterpret_cast<uintptr_t>(typeTag);
#if defined(_WIN64) || defined(__LP64__)
                std::snprintf(buf, static_cast<size_t>(bufSize), "0x%016llX",
                              static_cast<unsigned long long>(addr));
#else
                std::snprintf(buf, static_cast<size_t>(bufSize), "0x%08X",
                              static_cast<unsigned int>(addr));
#endif
            }

        } // anonymous namespace

        // --------------------------------------------------------------------
        // Construction
        // --------------------------------------------------------------------

        BlackboardVisualDebugger::BlackboardVisualDebugger(const Blackboard& blackboard)
            : mBlackboard(blackboard)
        {}

        // --------------------------------------------------------------------
        // IDebugDomain: identity
        // --------------------------------------------------------------------

        Dia::Core::StringCRC BlackboardVisualDebugger::GetDomainId() const
        {
            return Dia::Core::StringCRC("blackboard");
        }

        const char* BlackboardVisualDebugger::GetDisplayName() const
        {
            return "Blackboard";
        }

        const char* BlackboardVisualDebugger::GetDescription() const
        {
            return "Blackboard slots — key, type, and current value for each slot";
        }

        Dia::Core::StringCRC BlackboardVisualDebugger::GetGroup() const
        {
            return Dia::Core::StringCRC("AIBehavior");
        }

        Dia::Core::RGBA BlackboardVisualDebugger::GetAccentColour() const
        {
            return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior;
        }

        // --------------------------------------------------------------------
        // IDebugDomain: panel bridge
        // --------------------------------------------------------------------

        void BlackboardVisualDebugger::GetJSONState(Json::Value& out)
        {
            // Always emit the drawers array.
            const bool enabled = mSlotTableEnabled.load();

            Json::Value drawers(Json::arrayValue);
            {
                Json::Value entry(Json::objectValue);
                entry["name"]    = "SlotTable";
                entry["enabled"] = enabled;
                drawers.append(entry);
            }
            out["drawers"] = drawers;

            // Count slots and (optionally) build the slots array in one pass.
            int slotCount = 0;
            Json::Value slots(Json::arrayValue);

            mBlackboard.VisitSlots([&](Dia::Core::StringCRC key,
                                       const void*          typeTag,
                                       const void*          data)
            {
                ++slotCount;

                if (!enabled) return; // skip slot emission when drawer is disabled

                // key — use stored string if available, else CRC as hex
                const char* keyStr = key.AsChar();

                // type — typeTag rendered as hex pointer
                char typeBuf[32];
                FormatTypeTag(typeTag, typeBuf, static_cast<int>(sizeof(typeBuf)));

                // value — formatter if registered, else first-4-bytes hex fallback
                char valueBuf[64];
                const char* valueStr = nullptr;

                const unsigned int formatterCount = mFormatters.Size();
                for (unsigned int i = 0; i < formatterCount; ++i)
                {
                    if (mFormatters[i].typeTag == typeTag)
                    {
                        valueStr = mFormatters[i].fn(data, valueBuf, static_cast<int>(sizeof(valueBuf)));
                        break;
                    }
                }

                if (valueStr == nullptr)
                {
                    // Hex fallback: first 4 bytes of data
                    if (data != nullptr)
                    {
                        uint32_t raw = 0;
                        std::memcpy(&raw, data, sizeof(raw));
                        std::snprintf(valueBuf, sizeof(valueBuf), "0x%08X", raw);
                    }
                    else
                    {
                        std::snprintf(valueBuf, sizeof(valueBuf), "0x00000000");
                    }
                    valueStr = valueBuf;
                }

                Json::Value slotEntry(Json::objectValue);
                slotEntry["key"]   = (keyStr != nullptr && keyStr[0] != '\0') ? keyStr : "";
                slotEntry["type"]  = typeBuf;
                slotEntry["value"] = (valueStr != nullptr) ? valueStr : "";
                slots.append(slotEntry);
            });

            Json::Value stats(Json::objectValue);
            stats["slotCount"] = slotCount;
            out["stats"] = stats;

            if (enabled)
                out["slots"] = slots;
        }

        void BlackboardVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
        {
            if (cmd == kCmdToggle)
            {
                if (!args.isMember("drawer") || !args["drawer"].isString()) return;
                const Dia::Core::StringCRC drawerName(args["drawer"].asCString());
                if (drawerName == kDrawerSlotTable)
                    mSlotTableEnabled.store(!mSlotTableEnabled.load());
                return;
            }
            // "setScale" and all other commands — no-op, no crash
        }

        // --------------------------------------------------------------------
        // Formatter registration
        // --------------------------------------------------------------------

        void BlackboardVisualDebugger::RegisterFormatter(
            const void* typeTag,
            const char* (*fn)(const void* data, char* buf, int bufSize))
        {
            if (mFormatters.IsFull()) return;
            FormatterEntry entry;
            entry.typeTag = typeTag;
            entry.fn      = fn;
            mFormatters.Add(entry);
        }

    } // namespace Blackboard
} // namespace Dia

#endif // DIA_DEBUG
