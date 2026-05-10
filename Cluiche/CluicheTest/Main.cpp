#include <stdio.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ManifestComposerV2.h>
#include <DiaApplicationFlow/Manifest/ManifestValidatorV2.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <sstream>
#include <string>

// Force-link all v2 modules via DIA_MODULE registrations
// Each module's .cpp registers itself at static init time via DIA_MODULE.
// As long as those translation units are linked in, no explicit include needed.

namespace {

// Read the .diagame, extract config.path_aliases, and register them in PathStore
// relative to the .diagame's directory. Must be called before any module that
// resolves a FilePath (e.g. KernelModule creating a RenderWindow).
bool RegisterPathAliasesFromDiagame(const char* diagamePath)
{
    std::ifstream file(diagamePath);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string contents = ss.str();

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(contents, root))
        return false;

    // Compute baseDir = directory containing the .diagame, **including the
    // trailing slash**. Dia::Core::Path::ResolveRelative concatenates
    // baseDir + relativePath directly with no separator, so baseDir must
    // end in '/' for the result to be a valid path.
    char baseDir[512] = {0};
    const char* lastSlash = nullptr;
    for (const char* p = diagamePath; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\')
            lastSlash = p;
    }
    if (lastSlash)
    {
        unsigned int len = static_cast<unsigned int>(lastSlash - diagamePath) + 1; // include the slash
        if (len >= sizeof(baseDir)) len = sizeof(baseDir) - 1;
        for (unsigned int i = 0; i < len; ++i)
            baseDir[i] = diagamePath[i];
        baseDir[len] = '\0';
    }

    if (!root.isMember("config") || !root["config"].isObject())
        return true; // No config — not an error.

    const Json::Value& config = root["config"];
    if (!config.isMember("path_aliases") || !config["path_aliases"].isObject())
        return true;

    const Json::Value& aliases = config["path_aliases"];
    Json::Value::Members members = aliases.getMemberNames();
    for (unsigned int i = 0; i < members.size(); ++i)
    {
        const std::string& aliasName = members[i];
        const char* relPath = aliases[aliasName].asCString();

        Dia::Core::Containers::String512 resolved;
        Dia::Core::Path::ResolveRelative(baseDir, relPath, resolved);

        Dia::Core::Path::Alias alias(aliasName.c_str());
        Dia::Core::Path::String pathStr(resolved.AsCStr());
        Dia::Core::PathStore::RegisterToStore(alias, pathStr);
    }
    return true;
}

} // namespace

int main(int argc, const char* argv[])
{
    const char* kDiagamePath = "assets/cluichetest.diagame";

    // Register path aliases (config.path_aliases from .diagame) BEFORE any module
    // starts resolving FilePaths. KernelModule's RenderWindow needs "root" in
    // particular for shader lookup.
    if (!RegisterPathAliasesFromDiagame(kDiagamePath))
    {
        printf("Failed to register path aliases from .diagame\n");
        return 1;
    }

    // Compose manifest from .diagame (resolves imports, merges stages)
    Dia::ApplicationFlow::ApplicationManifestV2 manifest;
    Dia::ApplicationFlow::ComposeResult composeResult =
        Dia::ApplicationFlow::ManifestComposerV2::Compose(kDiagamePath, manifest);

    if (composeResult != Dia::ApplicationFlow::ComposeResult::kSuccess)
    {
        printf("Failed to compose manifest (result: %d)\n", static_cast<int>(composeResult));
        return 1;
    }

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

    // Create and run application
    Dia::ApplicationFlow::Application app(manifest, registry);

    if (!app.Start())
    {
        printf("Application failed to start\n");
        return 1;
    }

    // Main loop — runs at MainPU frequency (30Hz)
    // Application returns false when all modules are inactive (shutdown complete)
    const float kFrameTimeSec = 1.0f / 30.0f;
    while (app.Update(kFrameTimeSec))
    {
        // MainPU runs inline here. SimPU and RenderPU run on dedicated threads.
    }

    return 0;
}
