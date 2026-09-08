#pragma once

#include <DiaHTN/TaskResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdint>
#include <cstring>

namespace Dia
{
    namespace HTN
    {
        // Raw function pointer for plain operator implementations (no capture needed).
        // SD-002: Returns TaskResult for multi-tick operator lifecycle.
        using OperatorFn = TaskResult(*)(void* operatorContext,
                                         const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);

        //-------------------------------------------------------------------------------------------
        // OperatorBinding
        //
        // Fat callable that can hold either a plain OperatorFn or a capture-bearing function
        // (used by the RuleActionBridge adapter). Zero dynamic allocation — fits in 24 bytes.
        //
        // Callers constructing plain operators use the OperatorFn overload of Register and
        // never touch this struct directly. The bridge is the only producer of capture-bearing
        // bindings.
        //-------------------------------------------------------------------------------------------
        struct OperatorBinding
        {
            // 3-arg form used when a capture is needed (bridge adapters).
            // capture: opaque value stored at registration time (e.g. packed function pointer).
            using CaptureFn = TaskResult(*)(uintptr_t capture,
                                            void* operatorContext,
                                            const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);

            OperatorFn  plainFn   = nullptr;
            CaptureFn   captureFn = nullptr;
            uintptr_t   capture   = 0;

            OperatorBinding() = default;

            // Implicit construction from a plain OperatorFn.
            OperatorBinding(OperatorFn fn) // NOLINT(google-explicit-constructor)
                : plainFn(fn), captureFn(nullptr), capture(0) {}

            explicit operator bool() const { return plainFn != nullptr || captureFn != nullptr; }

            bool operator==(std::nullptr_t) const { return !static_cast<bool>(*this); }
            bool operator!=(std::nullptr_t) const { return  static_cast<bool>(*this); }

            TaskResult operator()(void* operatorContext,
                                  const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params) const
            {
                if (plainFn)
                    return plainFn(operatorContext, params);
                if (captureFn)
                    return captureFn(capture, operatorContext, params);
                return TaskResult::kFailed;
            }
        };

        //-------------------------------------------------------------------------------------------
        // OperatorRegistry
        //
        // Open handler table mapping StringCRC operator names to OperatorBinding callables.
        //
        // SD-009: Explicitly constructed — not a singleton. Caller creates, owns, passes by ref.
        // SD-002: OperatorFn returns TaskResult (kRunning/kSucceeded/kFailed) for multi-tick ops.
        // PD-001: All operator IDs use StringCRC.
        // PD-004: No STL in the public API — internal implementation uses std::unordered_map.
        // AD-003: All code in Dia::HTN:: namespace.
        //-------------------------------------------------------------------------------------------
        class OperatorRegistry
        {
        public:
            OperatorRegistry();
            ~OperatorRegistry();

            // Register a plain operator function (most callers use this).
            void Register(Dia::Core::StringCRC operatorId, OperatorFn fn);

            // Register a capture-bearing binding (used by bridge adapters).
            void Register(Dia::Core::StringCRC operatorId, OperatorBinding binding);

            // Returns an empty OperatorBinding (operator bool == false) if not found.
            OperatorBinding Find(Dia::Core::StringCRC operatorId) const;

            bool Has(Dia::Core::StringCRC operatorId) const;

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace HTN
} // namespace Dia
