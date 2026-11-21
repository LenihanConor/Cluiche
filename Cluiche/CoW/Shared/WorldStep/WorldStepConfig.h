// World Step config: stage order, apply delay and deferred flag.
// Spec: docs/specs/applications/cow/systems/worldstep/step-chain.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>

#include <cstdint>

namespace Json { class Value; }

namespace CoW::WorldStep
{
    constexpr uint32_t kMaxStages = 8;

    struct StageId
    {
        Dia::Core::StringCRC            crc;
        Dia::Core::Containers::String32 name;   // kept for logs and errors
    };

    using StageIdList = Dia::Core::Containers::DynamicArrayC<StageId, kMaxStages>;

    // Filled in on a failed load: which key, and why.
    struct ConfigError
    {
        Dia::Core::Containers::String32 key;
        Dia::Core::Containers::String64 reason;
    };

    struct WorldStepConfig
    {
        StageIdList order;                 // declared stage order; ids are strings in the file
        uint32_t    applyAfterDays = 7;
        bool        deferred       = true;
    };

    using StageHashFn = Dia::Core::StringCRC (*)(const char*);

    // All-or-nothing: on any fault returns false, leaves out equal to a default config, and names the key in err.
    bool LoadWorldStepConfig(const Json::Value& root, WorldStepConfig& out, ConfigError& err);

    // Collision seam: ids are hashed with `hash` (tests inject a colliding hash).
    bool LoadWorldStepConfig(const Json::Value& root, WorldStepConfig& out, ConfigError& err, StageHashFn hash);
}
