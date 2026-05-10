#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>

namespace Dia { namespace ApplicationFlow {

    enum class LoadResult { kSuccess, kFileNotFound, kParseError, kVersionMismatch };

    class ApplicationManifestLoaderV2 {
    public:
        // Load from file path — reads JSON, populates outManifest
        static LoadResult LoadFromFile(const char* filePath, ApplicationManifestV2& outManifest);

        // Load from JSON string — for testing
        static LoadResult LoadFromString(const char* jsonString, ApplicationManifestV2& outManifest);

    private:
        static LoadResult ParseJson(const char* jsonString, ApplicationManifestV2& outManifest);
    };

}} // namespace Dia::ApplicationFlow
