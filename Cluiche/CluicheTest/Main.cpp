#include <stdio.h>
#include <memory>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Manifest/ManifestBootstrap.h>
#include <DiaValueSchema/DescribeSession.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentSchemaJson.h>
#include <DiaEntity/IComponent.h>
#include <DiaCore/Metadata/DescriptionRegistry.h>
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaCore/Json/external/json/json.h>

// Force-link all v2 modules via DIA_MODULE registrations
// Each module's .cpp registers itself at static init time via DIA_MODULE.
// As long as those translation units are linked in, no explicit include needed.

namespace {

// ---------------------------------------------------------------------------
// --dump-schema helpers
// ---------------------------------------------------------------------------


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

            components.append(Dia::Entity::BuildComponentSchemaJson(desc));
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

    // --describe-schema <path> [--stage <name>]: DiaValueSchema describe mode. ValueSchemaModule writes the export once
    // every PU has finished Init, then requests shutdown; the exit code is returned below (see DescribeMode.h).
    if (!Dia::ValueSchema::BeginDescribeFromArgs(argc, argv, "cluichetest"))
        return 1;

    const char* kDiagamePath = "assets/cluichetest.diagame";

    // Heap-allocate manifest — the struct grows with module/channel count and
    // can overflow the default 1MB stack when all stage diaapps are merged.
    std::unique_ptr<Dia::ApplicationFlow::ApplicationManifestV3> manifestOwner(
        new Dia::ApplicationFlow::ApplicationManifestV3());
    Dia::ApplicationFlow::ApplicationManifestV3& manifest = *manifestOwner;
    Dia::ApplicationFlow::TypeRegistry& registry = Dia::ApplicationFlow::TypeRegistry::Global();

    // --dev-alias name=path (set by `dia launch`) must register before the manifest loads.
    Dia::ApplicationFlow::RegisterDevAliases(argc, argv);

    // Compose, register path aliases (before any module resolves a FilePath), validate.
    if (!Dia::ApplicationFlow::LoadManifest(kDiagamePath, manifest, registry))
        return 1;

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
    return Dia::ValueSchema::DescribeSession::FinalExitCode();  // 0 unless --describe-schema was requested
}
#pragma warning(pop)
