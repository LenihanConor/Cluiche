#include <stdio.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ManifestComposerV2.h>
#include <DiaApplicationFlow/Manifest/ManifestValidatorV2.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/Json/external/json/json.h>

// Force-link all v2 modules via DIA_MODULE registrations
// Each module's .cpp registers itself at static init time via DIA_MODULE.
// As long as those translation units are linked in, no explicit include needed.

namespace {

// Compute baseDir from a file path — everything up to and including the last
// separator, so that ResolveRelative can concatenate directly.
void ComputeBaseDir(const char* filePath, char* baseDir, unsigned int baseDirSize)
{
    const char* lastSlash = nullptr;
    for (const char* p = filePath; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\')
            lastSlash = p;
    }
    if (lastSlash)
    {
        unsigned int len = static_cast<unsigned int>(lastSlash - filePath) + 1;
        if (len >= baseDirSize) len = baseDirSize - 1;
        for (unsigned int i = 0; i < len; ++i)
            baseDir[i] = filePath[i];
        baseDir[len] = '\0';
    }
    else
    {
        baseDir[0] = '\0';
    }
}

// Register path_aliases from the already-composed manifest's diagameConfig.
// Must be called before any module that resolves a FilePath.
void RegisterPathAliases(const Dia::ApplicationFlow::ApplicationManifestV3& manifest,
                         const char* diagamePath)
{
    if (!manifest.diagameConfig) return;
    const Json::Value& config = *manifest.diagameConfig;
    if (!config.isMember("path_aliases") || !config["path_aliases"].isObject()) return;

    char baseDir[512] = {};
    ComputeBaseDir(diagamePath, baseDir, sizeof(baseDir));

    const Json::Value& aliases = config["path_aliases"];
    Json::Value::Members members = aliases.getMemberNames();
    for (unsigned int i = 0; i < static_cast<unsigned int>(members.size()); ++i)
    {
        const std::string& aliasName = members[i];
        const char* relPath = aliases[aliasName].asCString();

        Dia::Core::Containers::String512 resolved;
        Dia::Core::Path::ResolveRelative(baseDir, relPath, resolved);

        Dia::Core::Path::Alias alias(aliasName.c_str());
        Dia::Core::Path::String pathStr(resolved.AsCStr());
        Dia::Core::PathStore::RegisterToStore(alias, pathStr);
    }
}

} // namespace

#pragma warning(push)
#pragma warning(disable: 6262)  // Application + manifest stack frame is large but main() is a one-shot
int main(int argc, const char* argv[])
{
    const char* kDiagamePath = "assets/cluichetest.diagame";

    // Compose manifest from .diagame (resolves imports, merges stages, captures config)
    Dia::ApplicationFlow::ApplicationManifestV3 manifest;
    Dia::ApplicationFlow::ComposeResult composeResult =
        Dia::ApplicationFlow::ManifestComposerV2::Compose(kDiagamePath, manifest);

    if (composeResult != Dia::ApplicationFlow::ComposeResult::kSuccess)
    {
        printf("Failed to compose manifest (result: %d)\n", static_cast<int>(composeResult));
        return 1;
    }

    // Register path aliases from diagameConfig BEFORE any module resolves a FilePath.
    RegisterPathAliases(manifest, kDiagamePath);

    // Validate manifest against registered types
    Dia::ApplicationFlow::TypeRegistry& registry = Dia::ApplicationFlow::TypeRegistry::Global();
    Dia::ApplicationFlow::ManifestValidatorV2 validator(registry);
    validator.Validate(manifest);

    if (validator.HasErrors())
    {
        const auto& results = validator.GetResults();
        for (unsigned int i = 0; i < results.Size(); ++i)
        {
            printf("Validation error: %s\n", results[i].message.AsCStr());
        }
        return 1;
    }

    // Create session-scoped test results registry
    CluicheTest::TestResultsRegistry::Create();

    // Create and run application
    Dia::ApplicationFlow::Application app(manifest, registry);

    if (!app.Start())
    {
        printf("Application failed to start\n");
        CluicheTest::TestResultsRegistry::Destroy();
        return 1;
    }

    // Main loop — runs at MainPU frequency (30Hz)
    // Application returns false when all modules are inactive (shutdown complete)
    const float kFrameTimeSec = 1.0f / 30.0f;
    while (app.Update(kFrameTimeSec))
    {
        // MainPU runs inline here. SimPU and RenderPU run on dedicated threads.
    }

    CluicheTest::TestResultsRegistry::Destroy();
    return 0;
}
#pragma warning(pop)
