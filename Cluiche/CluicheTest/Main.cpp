#include <stdio.h>
#include <memory>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Manifest/ManifestComposerV2.h>
#include <DiaApplicationFlow/Manifest/ManifestValidatorV2.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/IComponent.h>
#include <DiaCore/Metadata/DescriptionRegistry.h>
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/Json/external/json/json.h>

// Force-link all v2 modules via DIA_MODULE registrations
// Each module's .cpp registers itself at static init time via DIA_MODULE.
// As long as those translation units are linked in, no explicit include needed.

namespace {

// ---------------------------------------------------------------------------
// --dump-schema helpers
// ---------------------------------------------------------------------------

// Convert a FieldKind enum to its lowercase JSON string representation.
const char* FieldKindToString(Dia::Entity::FieldKind kind)
{
    switch (kind)
    {
    case Dia::Entity::FieldKind::Primitive:    return "primitive";
    case Dia::Entity::FieldKind::StringId:     return "string_id";
    case Dia::Entity::FieldKind::Math:         return "math";
    case Dia::Entity::FieldKind::AssetHandle:  return "asset_handle";
    case Dia::Entity::FieldKind::EntityRef:    return "entity_ref";
    case Dia::Entity::FieldKind::Nested:       return "nested";
    case Dia::Entity::FieldKind::Container:    return "container";
    default:                                   return "unknown";
    }
}

// Convert a PUAffinity bitmask to a human-readable string tag.
// Combinations are joined with '|' (e.g. "main|sim").
std::string PUAffinityToString(Dia::ApplicationFlow::PUAffinity affinity)
{
    using Dia::ApplicationFlow::PUAffinity;
    const uint8_t val = static_cast<uint8_t>(affinity);

    if (val == static_cast<uint8_t>(PUAffinity::kNone)) return "none";
    if ((affinity & PUAffinity::kAny) == PUAffinity::kAny)  return "any";

    std::string result;
    auto append = [&](const char* tag) {
        if (!result.empty()) result += '|';
        result += tag;
    };

    if (Dia::ApplicationFlow::HasAffinity(affinity, PUAffinity::kMain))   append("main");
    if (Dia::ApplicationFlow::HasAffinity(affinity, PUAffinity::kSim))    append("sim");
    if (Dia::ApplicationFlow::HasAffinity(affinity, PUAffinity::kRender)) append("render");

    return result;
}

// Walk all registered component/module types and write the schema JSON to stdout.
// Returns 0 on success.
int DumpSchema()
{
    Json::Value root(Json::objectValue);

    // version
    Json::Value version(Json::objectValue);
    version["major"] = 1;
    version["minor"] = 0;
    root["version"] = version;

    // game identifier
    root["game"] = "cluichetest";

    // components — sourced from Dia::Entity::ComponentRegistry
    Json::Value components(Json::arrayValue);
    {
        Dia::Entity::ComponentRegistry& reg = Dia::Entity::ComponentRegistry::Get();
        const uint32_t count = reg.GetCount();
        for (uint32_t i = 0; i < count; ++i)
        {
            const Dia::Entity::ComponentTypeDesc& desc = reg.GetByIndex(i);

            Json::Value comp(Json::objectValue);

            // type_id: prefer the string stored in the StringCRC; fall back to hex CRC value.
            const char* typeIdStr = desc.typeId.AsChar();
            if (typeIdStr && typeIdStr[0] != '\0')
            {
                comp["type_id"] = typeIdStr;
            }
            else
            {
                char hexBuf[20];
                snprintf(hexBuf, sizeof(hexBuf), "0x%08X", desc.typeId.Value());
                comp["type_id"] = hexBuf;
            }

            comp["debug_name"] = desc.debugName ? desc.debugName : "";

            const char* compDesc = Dia::Core::Metadata::DescriptionRegistry::Get(desc.typeId);
            comp["description"] = compDesc ? compDesc : "";

            Json::Value fields(Json::arrayValue);
            for (uint16_t f = 0; f < desc.fieldCount; ++f)
            {
                const Dia::Entity::FieldDesc& fd = desc.fields[f];
                Json::Value field(Json::objectValue);
                field["name"] = fd.name ? fd.name : "";
                field["kind"] = FieldKindToString(fd.kind);
                fields.append(field);
            }
            comp["fields"] = fields;

            // default_values: serialise a zero-initialised default instance via saveToJson.
            Json::Value defaultValuesJson(Json::objectValue);
            if (desc.saveToJson != nullptr)
            {
                // Guard against zero alignment (shouldn't happen, but be safe).
                const size_t alignment = (desc.alignment > 0)
                    ? static_cast<size_t>(desc.alignment)
                    : alignof(std::max_align_t);

                void* buf = _aligned_malloc(static_cast<size_t>(desc.size), alignment);
                if (buf)
                {
                    memset(buf, 0, static_cast<size_t>(desc.size));
                    const Dia::Entity::IComponent* ptr =
                        static_cast<const Dia::Entity::IComponent*>(buf);
                    desc.saveToJson(ptr, defaultValuesJson);
                    _aligned_free(buf);
                }
            }
            else
            {
                fprintf(stderr,
                    "reflect: WARNING: %s has no saveToJson — default_values will be zero-filled\n",
                    desc.debugName ? desc.debugName : "unknown");

                for (uint16_t f = 0; f < desc.fieldCount; ++f)
                {
                    const Dia::Entity::FieldDesc& fd = desc.fields[f];
                    const char* fieldName = fd.name ? fd.name : "";
                    switch (fd.kind)
                    {
                    case Dia::Entity::FieldKind::Primitive:
                        defaultValuesJson[fieldName] = Json::Value(Json::Int(0));
                        break;
                    case Dia::Entity::FieldKind::StringId:
                        defaultValuesJson[fieldName] = Json::Value("");
                        break;
                    case Dia::Entity::FieldKind::Math:
                        defaultValuesJson[fieldName] = Json::Value(Json::Int(0));
                        break;
                    case Dia::Entity::FieldKind::AssetHandle:
                        defaultValuesJson[fieldName] = Json::Value("");
                        break;
                    case Dia::Entity::FieldKind::EntityRef:
                        defaultValuesJson[fieldName] = Json::Value("");
                        break;
                    case Dia::Entity::FieldKind::Nested:
                        defaultValuesJson[fieldName] = Json::Value(Json::objectValue);
                        break;
                    case Dia::Entity::FieldKind::Container:
                        defaultValuesJson[fieldName] = Json::Value(Json::arrayValue);
                        break;
                    default:
                        defaultValuesJson[fieldName] = Json::Value(Json::Int(0));
                        break;
                    }
                }
            }
            comp["default_values"] = defaultValuesJson;

            components.append(comp);
        }
    }
    root["components"] = components;

    // modules — sourced from Dia::ApplicationFlow::TypeRegistry
    Json::Value modules(Json::arrayValue);
    {
        Dia::ApplicationFlow::TypeRegistry& reg = Dia::ApplicationFlow::TypeRegistry::Global();
        reg.ForEach([&](const Dia::Core::StringCRC& typeId,
                        const Dia::ApplicationFlow::TypeRegistry::TypeMetadata& meta)
        {
            Json::Value mod(Json::objectValue);
            mod["type_id"]     = typeId.AsChar() ? typeId.AsChar() : "";
            mod["description"] = meta.description ? meta.description : "";
            mod["allowed_pus"] = PUAffinityToString(meta.allowedPUs);
            modules.append(mod);
        });
    }
    root["modules"] = modules;

    // processing_units — PU types are not yet registered in TypeRegistry.
    // TODO: PU types not yet registered in TypeRegistry; populated by CLI (T2) from manifest.
    root["processing_units"] = Json::Value(Json::arrayValue);

    // Write indented JSON to stdout.
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &std::cout);
    std::cout << std::endl;

    return 0;
}

// ---------------------------------------------------------------------------
// Path / manifest helpers
// ---------------------------------------------------------------------------

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
#pragma warning(disable: 6262)  // Application stack frame is large but main() is a one-shot
int main(int argc, const char* argv[])
{
    // Early-exit: --dump-schema writes registered-types JSON to stdout and exits.
    // Must run before any manifest loading, path alias setup, or Application creation.
    // --automation: passed by the E2E test runner; enables timed auto-exit after stages resolve.
    bool automationMode = false;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] && strcmp(argv[i], "--dump-schema") == 0)
            return DumpSchema();
        if (argv[i] && strcmp(argv[i], "--automation") == 0)
            automationMode = true;
    }

    const char* kDiagamePath = "assets/cluichetest.diagame";

    // Heap-allocate manifest — the struct grows with module/channel count and
    // can overflow the default 1MB stack when all stage diaapps are merged.
    std::unique_ptr<Dia::ApplicationFlow::ApplicationManifestV3> manifestOwner(
        new Dia::ApplicationFlow::ApplicationManifestV3());
    Dia::ApplicationFlow::ApplicationManifestV3& manifest = *manifestOwner;
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
    CluicheTest::TestResultsRegistry::SetAutomationMode(automationMode);

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
