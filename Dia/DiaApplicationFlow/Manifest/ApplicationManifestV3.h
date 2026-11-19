#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>
#include <DiaStreams/OverflowPolicy.h>

namespace Json { class Value; }

namespace Dia { namespace ApplicationFlow {

    // Describes a data stream connecting two processing units
    struct StreamDeclaration
    {
        Dia::Core::StringCRC id;

        // v2.1 fields
        Dia::Core::StringCRC kind;         // "EventStream", "FrameStream", or "ServiceStream"
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

    // Describes a module's binding to a stream channel (unified reads/writes/provides/consumes)
    struct ChannelBinding
    {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC role;  // "reads", "writes", "provides", "consumes"
    };

    // Describes a single module instance within a processing unit
    struct ModuleDeclaration
    {
        Dia::Core::StringCRC instanceId;
        Dia::Core::StringCRC typeId;

        // Stage names this module is active in. Use StringCRC("all") as a sentinel meaning always active.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> stages;

        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>  dependencies;
        Dia::Core::Containers::DynamicArrayC<ChannelBinding, 16>       channels;

        // Optional tick-order declaration: instance ids of modules in the same PU that must be listed earlier.
        // Enforced by `dia validate manifest` only; the runtime ignores it. Carried so the editor round-trips it.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>  requiresTickAfter;

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
        unsigned int maxCatchUpTicksPerFrame = 5;

        Dia::Core::Containers::DynamicArrayC<ModuleDeclaration, 64> modules;
    };

    // Launch-preset limits (launch-presets spec AC 1-2)
    static const unsigned int kMaxPresetsPerStage       = 16;
    static const unsigned int kMaxPresetNameLength      = 31;
    static const unsigned int kMaxPresetDescLength      = 120;
    static const unsigned int kMaxPresetJsonBytes       = 2048;  // per preset (compact JSON of its "modules" object)
    static const unsigned int kMaxPresetModules         = 4;
    static const unsigned int kMaxManifestPresets       = 64;    // across all stages of one manifest
    static const unsigned int kPresetOverridePoolBytes  = 32768; // shared override text pool for the whole manifest

    // One module's override inside a preset. Text lives in ApplicationManifestV3's shared pool.
    struct PresetModuleOverride
    {
        Dia::Core::StringCRC instanceId;
        unsigned int         offset = 0;  // into the manifest override pool
        unsigned int         length = 0;  // bytes, excluding terminator
    };

    // A named launch preset: per-module config overrides applied at stage entry
    struct PresetDeclaration
    {
        Dia::Core::StringCRC name;
        char                 description[kMaxPresetDescLength + 1] = {};
        unsigned int         moduleCount = 0;
        PresetModuleOverride modules[kMaxPresetModules];
    };

    // Describes a stage entry (from .diastage format)
    struct StageDeclaration
    {
        Dia::Core::StringCRC                    name;
        Dia::Core::Containers::String256        manifestPath;

        // v3 fields: per-stage transition targets and auto-advance flag
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>  transitions;
        bool                                                            autoAdvance = false;

        // Launch presets: range into ApplicationManifestV3's shared preset table (compact; see
        // ApplicationManifestV3::FindPreset). Empty when the stage declares none.
        unsigned int                                                    presetFirst = 0;
        unsigned int                                                    presetCount = 0;
    };

    // Top-level in-memory representation of a v3 .diaapp manifest
    struct ApplicationManifestV3
    {
        int version = 3;

        Dia::Core::Containers::DynamicArrayC<StageDeclaration, 32>          stages;
        Dia::Core::StringCRC                                                 initialStage;

        Dia::Core::Containers::DynamicArrayC<StreamDeclaration, 64>         streams;
        Dia::Core::Containers::DynamicArrayC<ProcessingUnitDeclaration, 4>  processingUnits;

        // Full "config" block from the .diagame file — heap-allocated, owned here.
        // Null if no .diagame config was present or Compose was not used.
        Json::Value* diagameConfig = nullptr;

        // Launch presets for all stages (compact shared storage; StageDeclaration holds a range).
        Dia::Core::Containers::DynamicArrayC<PresetDeclaration, kMaxManifestPresets> presets;
        char         presetOverridePool[kPresetOverridePoolBytes] = {};
        unsigned int presetOverridePoolUsed = 0;

        // Preset accessors. FindPreset returns nullptr if the stage or preset does not exist.
        const PresetDeclaration* FindPreset(const StageDeclaration& stage, const Dia::Core::StringCRC& presetName) const;
        const PresetDeclaration* FindPreset(const Dia::Core::StringCRC& stageName, const Dia::Core::StringCRC& presetName) const;
        bool                     HasPreset(const Dia::Core::StringCRC& stageName, const Dia::Core::StringCRC& presetName) const;
        // Override JSON text for a module in a preset; "" if the module has no override.
        const char*              GetPresetOverrideJson(const PresetDeclaration& preset, const Dia::Core::StringCRC& instanceId) const;

        ApplicationManifestV3() = default;
        ~ApplicationManifestV3();
        ApplicationManifestV3(const ApplicationManifestV3&) = delete;
        ApplicationManifestV3& operator=(const ApplicationManifestV3&) = delete;
        ApplicationManifestV3(ApplicationManifestV3&& other) noexcept;
        ApplicationManifestV3& operator=(ApplicationManifestV3&& other) noexcept;
    };

}} // namespace Dia::ApplicationFlow
