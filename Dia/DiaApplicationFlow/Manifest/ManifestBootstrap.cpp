#include <DiaApplicationFlow/Manifest/ManifestBootstrap.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ManifestComposerV2.h>
#include <DiaApplicationFlow/Manifest/ManifestValidatorV2.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace ApplicationFlow {

    namespace
    {
        // Everything up to and including the last separator, so that ResolveRelative
        // can concatenate directly.
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
    }

    void RegisterDevAliases(int argc, const char* const argv[])
    {
        static const char* const kFlag = "--dev-alias";
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::strcmp(argv[i], kFlag) != 0) continue;

            const char* arg = argv[++i];
            const char* eq = std::strchr(arg, '=');
            if (!eq || eq == arg || eq[1] == 0) continue;

            const std::string name(arg, static_cast<size_t>(eq - arg));
            const char* path = eq + 1;
            if (name.size() >= Dia::Core::StringCRC::kStringLength || std::strlen(path) >= Dia::Core::Path::String::Capacity()) continue;

            Dia::Core::Path::Alias alias(name.c_str());
            if (Dia::Core::PathStore::IsPathAliasRegistered(alias))
                Dia::Core::PathStore::UnregisterFromStore(alias);
            Dia::Core::PathStore::RegisterToStore(alias, Dia::Core::Path::String(path));
        }
    }

    void RegisterPathAliases(const ApplicationManifestV3& manifest, const char* diagamePath)
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
            if (aliasName == "source")
            {
                DIA_LOG_ERROR("ManifestBootstrap", "Manifest path_aliases may not set reserved alias 'source' (only --dev-alias); ignoring");
                continue;
            }
            const char* relPath = aliases[aliasName].asCString();

            Dia::Core::Containers::String512 resolved;
            Dia::Core::Path::ResolveRelative(baseDir, relPath, resolved);

            Dia::Core::Path::Alias alias(aliasName.c_str());
            Dia::Core::Path::String pathStr(resolved.AsCStr());
            if (Dia::Core::PathStore::IsPathAliasRegistered(alias))
                Dia::Core::PathStore::UnregisterFromStore(alias); // manifest wins over dev aliases
            Dia::Core::PathStore::RegisterToStore(alias, pathStr);
        }
    }

    bool LoadManifest(const char* diagamePath, ApplicationManifestV3& outManifest, TypeRegistry& registry)
    {
        const ComposeResult composeResult = ManifestComposerV2::Compose(diagamePath, outManifest);
        if (composeResult != ComposeResult::kSuccess)
        {
            printf("Failed to compose manifest (result: %d)\n", static_cast<int>(composeResult));
            return false;
        }

        RegisterPathAliases(outManifest, diagamePath);

        ManifestValidatorV2 validator(registry);
        validator.Validate(outManifest);

        if (validator.HasErrors())
        {
            const auto& results = validator.GetResults();
            for (unsigned int i = 0; i < results.Size(); ++i)
            {
                const bool isError = results[i].severity == ValidationSeverity::kError;
                printf("Validation %s: %s\n", isError ? "error" : "warning", results[i].message.AsCStr());
            }
            return false;
        }
        return true;
    }

}} // namespace Dia::ApplicationFlow
