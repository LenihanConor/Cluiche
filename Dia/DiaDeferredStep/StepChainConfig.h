// Step chain config: stage order, apply delay and deferred flag. Capacity is a template parameter
// so storage stays inline (no sim-thread allocation).
// Spec: docs/specs/applications/dia/systems/diadeferredstep/step-chain.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>

#include <cstdint>

namespace Dia::DeferredStep
{
    constexpr uint32_t kDefaultStageCapacity = 8;

    struct StageId
    {
        Dia::Core::StringCRC            crc;
        Dia::Core::Containers::String32 name;   // kept for logs and errors
    };

    template <uint32_t Capacity = kDefaultStageCapacity>
    using StageIdList = Dia::Core::Containers::DynamicArrayC<StageId, Capacity>;

    // Filled in on a failed load: which key, and why.
    struct ConfigError
    {
        Dia::Core::Containers::String32 key;
        Dia::Core::Containers::String64 reason;
    };

    template <uint32_t Capacity = kDefaultStageCapacity>
    struct StepChainConfig
    {
        StageIdList<Capacity> order;               // declared stage order; ids are strings in the file
        uint32_t              applyAfterSteps = 7;
        bool                  deferred        = true;
    };

    using StageHashFn = Dia::Core::StringCRC (*)(const char*);
}
