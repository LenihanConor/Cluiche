#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>

namespace Dia { namespace ApplicationFlow {

    // Describes a data stream connecting two processing units
    struct StreamDeclaration
    {
        Dia::Core::StringCRC id;

        // v2.1 fields
        Dia::Core::StringCRC kind;         // "EventStream" or "FrameStream"
        Dia::Core::StringCRC payloadType;  // e.g. "InputEvent" — must match DIA_STREAM_TYPE registration

        Dia::Core::StringCRC fromPU;
        Dia::Core::StringCRC toPU;
        bool multiWriter = false;

        // Per-stream capacity and reader cap (optional in manifest; 0 = use defaults)
        unsigned int capacity   = 0;
        unsigned int maxReaders = 0;

        // F3 policy fields (EventStream only; ignored for FrameStream)
        OverflowPolicy overflowPolicy  = OverflowPolicy::kDropOldest;
        unsigned int   blockTimeoutMs  = 100;
    };

    // Describes a single module instance within a processing unit
    struct ModuleDeclaration
    {
        Dia::Core::StringCRC instanceId;
        Dia::Core::StringCRC typeId;

        // Stage names this module is active in. Use StringCRC("all") as a sentinel meaning always active.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> stages;

        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>  dependencies;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>  reads;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>  writes;

        // Raw JSON config blob for this module (avoids Json::Value dependency in the header)
        Dia::Core::Containers::String256 configJson;

        float startTimeoutMs = 10000.0f;
        float stopTimeoutMs  = 5000.0f;
    };

    // Describes a processing unit and its contained modules
    struct ProcessingUnitDeclaration
    {
        Dia::Core::StringCRC instanceId;
        float frequencyHz    = 30.0f;
        bool  dedicatedThread = false;

        Dia::Core::Containers::DynamicArrayC<ModuleDeclaration, 32> modules;
    };

    // Describes a stage entry (from .diastage format)
    struct StageDeclaration
    {
        Dia::Core::StringCRC                    name;
        Dia::Core::Containers::String256        manifestPath;

        // v3 fields: per-stage transition targets and auto-advance flag
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>  transitions;
        bool                                                            autoAdvance = false;
    };

    // Top-level in-memory representation of a v3 .diaapp manifest
    struct ApplicationManifestV3
    {
        int version = 3;

        Dia::Core::Containers::DynamicArrayC<StageDeclaration, 16>          stages;
        Dia::Core::StringCRC                                                 initialStage;

        Dia::Core::Containers::DynamicArrayC<StreamDeclaration, 16>         streams;
        Dia::Core::Containers::DynamicArrayC<ProcessingUnitDeclaration, 4>  processingUnits;
    };

}} // namespace Dia::ApplicationFlow
