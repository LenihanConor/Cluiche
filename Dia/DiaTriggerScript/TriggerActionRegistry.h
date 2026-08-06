#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaTriggerScript/ITriggerActionHandler.h>

namespace Dia
{
    namespace TriggerScript
    {
        //-------------------------------------------------------------------------------------------
        // TriggerActionRegistry
        //
        // Maps StringCRC action type names to ITriggerActionHandler instances.
        //
        // SD-004: Explicitly constructed — not a singleton. Caller creates, owns, passes by ref.
        // PD-001: All action type IDs use StringCRC.
        // PD-004: No STL in the public API — internal implementation uses std::unordered_map.
        // AD-003: All code in Dia::TriggerScript:: namespace.
        //-------------------------------------------------------------------------------------------
        class TriggerActionRegistry
        {
        public:
            TriggerActionRegistry();
            ~TriggerActionRegistry();

            void Register(Dia::Core::StringCRC actionType, ITriggerActionHandler* handler);
            bool Has(Dia::Core::StringCRC actionType) const;

            // Finds handler by type and calls Execute(). DIA_ASSERTs if unregistered.
            void Dispatch(Dia::Core::StringCRC actionType, const ActionContext& ctx) const;

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace TriggerScript
} // namespace Dia
