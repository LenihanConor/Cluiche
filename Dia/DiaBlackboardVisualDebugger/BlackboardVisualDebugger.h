////////////////////////////////////////////////////////////////////////////////
// Filename: BlackboardVisualDebugger.h
// Description: IDebugDomain implementation for DiaBlackboard.
//              Panel-only (no world drawers). Emits slot table (key, type, value)
//              via VisitSlots. Supports per-type display formatters via
//              RegisterFormatter. Entire module is #ifdef DIA_DEBUG guarded.
// System spec: docs/specs/applications/dia/systems/diablackboardvisualdebugger/diablackboardvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <atomic>

namespace Dia { namespace Blackboard { class Blackboard; } }

namespace Dia
{
    namespace Blackboard
    {
        ////////////////////////////////////////////////////////////////////////////////
        // BlackboardVisualDebugger
        //
        // Panel-only IDebugDomain that wraps a const Blackboard& and emits
        // its slot state (key, type, value) to the DiaDebugPanel via GetJSONState().
        //
        // - No world-space drawers (HasWorldDrawers() == false).
        // - Per-type display formatters may be registered via RegisterFormatter().
        //   Unregistered types fall back to first-4-bytes hex ("0x????????").
        // - mSlotTableEnabled is std::atomic<bool>: OnCommand arrives on Render PU,
        //   GetJSONState runs on Sim PU.
        ////////////////////////////////////////////////////////////////////////////////
        class BlackboardVisualDebugger : public Dia::VisualDebugger::IDebugDomain
        {
        public:
            /// blackboard must outlive this object.
            explicit BlackboardVisualDebugger(const Blackboard& blackboard);

            // ---- IDebugDomain: identity ----
            Dia::Core::StringCRC GetDomainId()     const override; ///< "blackboard"
            const char*          GetDisplayName()  const override; ///< "Blackboard"
            const char*          GetDescription()  const override; ///< "Blackboard slots — key, type, and current value for each slot"
            Dia::Core::StringCRC GetGroup()        const override; ///< "AIBehavior"
            Dia::Core::RGBA      GetAccentColour() const override; ///< DebugGroupAccents::kAIBehavior

            bool HasWorldDrawers() const override { return false; }

            // ---- IDebugDomain: panel bridge ----
            void GetJSONState(Json::Value& out) override;
            void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

            // ---- Formatter registration ----
            /// Register a per-type display formatter.
            /// fn signature: const char* fn(const void* data, char* buf, int bufSize)
            /// typeTag must match the typeTag stored in the Blackboard slot.
            /// Max kMaxFormatters entries; excess registrations are silently dropped.
            void RegisterFormatter(const void* typeTag,
                                   const char* (*fn)(const void* data, char* buf, int bufSize));

        private:
            const Blackboard& mBlackboard;
            std::atomic<bool> mSlotTableEnabled{true};

            static constexpr int kMaxFormatters = 16;
            struct FormatterEntry
            {
                const void* typeTag;
                const char* (*fn)(const void* data, char* buf, int bufSize);
            };
            Dia::Core::Containers::DynamicArrayC<FormatterEntry, kMaxFormatters> mFormatters;
        };

    } // namespace Blackboard
} // namespace Dia

#endif // DIA_DEBUG
