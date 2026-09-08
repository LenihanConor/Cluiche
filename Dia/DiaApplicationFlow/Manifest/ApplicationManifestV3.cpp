#include "ApplicationManifestV3.h"
#include <DiaCore/Json/external/json/json.h>

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
{
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
        other.diagameConfig = nullptr;
    }
    return *this;
}

}} // namespace Dia::ApplicationFlow
