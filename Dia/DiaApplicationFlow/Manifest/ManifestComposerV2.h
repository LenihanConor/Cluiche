#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>

namespace Dia { namespace ApplicationFlow {

    enum class ComposeResult { kSuccess, kFileNotFound, kParseError, kMergeError };

    class ManifestComposerV2
    {
    public:
        // Entry point: reads .diagame, resolves imports, returns merged manifest
        static ComposeResult Compose(const char* diagamePath, ApplicationManifestV3& outManifest);

    private:
        static ComposeResult LoadDiagame(const char* path, ApplicationManifestV3& outManifest, char* baseDir);
        static ComposeResult MergeStage(const char* diastagePath, const char* baseDir, ApplicationManifestV3& outManifest);
        static bool ReadFile(const char* path, char* buffer, unsigned int bufferSize);
        static void BuildPath(const char* baseDir, const char* relPath, char* outPath, unsigned int outSize);
    };

}} // namespace Dia::ApplicationFlow
