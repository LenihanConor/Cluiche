#include "ApplicationManifestV3.h"
#include <DiaCore/Json/external/json/json.h>
#include <string.h>

namespace Dia { namespace ApplicationFlow {

ApplicationManifestV3::~ApplicationManifestV3()
{
    delete diagameConfig;
}

ApplicationManifestV3::ApplicationManifestV3(ApplicationManifestV3&& other) noexcept
    : version(other.version)
    , stages(other.stages)
    , initialStage(other.initialStage)
    , streams(other.streams)
    , processingUnits(other.processingUnits)
    , diagameConfig(other.diagameConfig)
    , presets(other.presets)
    , presetOverridePoolUsed(other.presetOverridePoolUsed)
{
    memcpy(presetOverridePool, other.presetOverridePool, other.presetOverridePoolUsed);
    other.diagameConfig = nullptr;
}

ApplicationManifestV3& ApplicationManifestV3::operator=(ApplicationManifestV3&& other) noexcept
{
    if (this != &other)
    {
        delete diagameConfig;
        version         = other.version;
        stages          = other.stages;
        initialStage    = other.initialStage;
        streams         = other.streams;
        processingUnits = other.processingUnits;
        diagameConfig   = other.diagameConfig;
        presets         = other.presets;
        presetOverridePoolUsed = other.presetOverridePoolUsed;
        memcpy(presetOverridePool, other.presetOverridePool, other.presetOverridePoolUsed);
        other.diagameConfig = nullptr;
    }
    return *this;
}

const PresetDeclaration* ApplicationManifestV3::FindPreset(const StageDeclaration& stage, const Dia::Core::StringCRC& presetName) const
{
    for (unsigned int i = 0; i < stage.presetCount; ++i)
    {
        const unsigned int idx = stage.presetFirst + i;
        if (idx >= presets.Size())
            break;
        if (presets[idx].name == presetName)
            return &presets[idx];
    }
    return nullptr;
}

const PresetDeclaration* ApplicationManifestV3::FindPreset(const Dia::Core::StringCRC& stageName, const Dia::Core::StringCRC& presetName) const
{
    for (unsigned int i = 0; i < stages.Size(); ++i)
    {
        if (stages[i].name == stageName)
            return FindPreset(stages[i], presetName);
    }
    return nullptr;
}

bool ApplicationManifestV3::HasPreset(const Dia::Core::StringCRC& stageName, const Dia::Core::StringCRC& presetName) const
{
    return FindPreset(stageName, presetName) != nullptr;
}

const char* ApplicationManifestV3::GetPresetOverrideJson(const PresetDeclaration& preset, const Dia::Core::StringCRC& instanceId) const
{
    for (unsigned int i = 0; i < preset.moduleCount; ++i)
    {
        if (preset.modules[i].instanceId == instanceId)
            return presetOverridePool + preset.modules[i].offset;
    }
    return "";
}

}} // namespace Dia::ApplicationFlow
