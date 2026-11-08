#include "Modules/WorldGenerationModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaCore/Json/external/json/json.h>
#include <functional>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace CoW {

const Dia::Core::StringCRC WorldGenerationModule::kTypeId("WorldGenerationModule");

WorldGenerationModule::WorldGenerationModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

WorldGenerationModule::~WorldGenerationModule()
{
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
}

void WorldGenerationModule::OnConfigure(const char* configJson)
{
    if (configJson != nullptr && configJson[0] != '\0')
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(configJson, root) || !root.isObject())
        {
            DIA_LOG_WARNING("CoW", "WorldGenerationModule: config is not a JSON object, using defaults");
        }
        else
        {
            if (root.isMember("presets_dir"))
            {
                if (root["presets_dir"].isString())
                    mPresetsDir = root["presets_dir"].asString();
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: presets_dir must be a string, ignored");
            }
            if (root.isMember("out_presets_dir"))
            {
                if (root["out_presets_dir"].isString())
                    mOutPresetsDir = root["out_presets_dir"].asString();
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: out_presets_dir must be a string, ignored");
            }
            if (root.isMember("auto_generate"))
            {
                if (root["auto_generate"].isBool())
                    mAutoGenerate = root["auto_generate"].asBool();
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: auto_generate must be a bool, ignored");
            }
            if (root.isMember("size"))
            {
                Json::Value args(Json::objectValue);
                args["size"] = root["size"];
                const Json::Value result = mSession.SetSize(args);
                if (result.isMember("error"))
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: invalid size in config, keeping default");
            }
            if (root.isMember("seed"))
            {
                if (root["seed"].isUInt64() || (root["seed"].isIntegral() && root["seed"].asInt64() >= 0))
                {
                    Json::Value args(Json::objectValue);
                    args["seed"] = root["seed"];
                    mSession.Set(args);
                }
                else
                {
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: seed must be an unsigned integer, ignored");
                }
            }
        }
    }
    mSession.SetPresetDirectories(mPresetsDir, mOutPresetsDir);
}

Dia::ApplicationFlow::StartResult WorldGenerationModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    DIA_TRACE_ZONE("WorldGenerationModule::DoStart", Dia::Observation::Trace::Category::kNone);

    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    RandomStreamModule* rng = mRandom.Get();
    if (!host || !rng || !rng->GetWorldStream())
        return StartResult::kLoading;

    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    mGenerationsCounter    = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.generations"));
    mPublishDroppedCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.publish_dropped"));
    mLastMsGauge           = metrics.RegisterGauge(Dia::Core::StringCRC("cow.worldgen.last_ms"));
    mRevisionGauge         = metrics.RegisterGauge(Dia::Core::StringCRC("cow.worldgen.revision"));

    // GameSpeedControllerModule also registers this type; RegisterType is an idempotent no-op
    // (returns false) when already registered, so start order does not matter.
    Dia::MessageBus::Bus& bus = host->GetBus();
    bus.RegisterType<WorldGenerationComplete, 4>();
    bus.RegisterProducer<WorldGenerationComplete>(GetInstanceId());

    mPublishFailed = false;
    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealthRegistered = true;
    mHealth.SetOK();

    RegisterCommands();

    DIA_LOG_INFO("CoW", "WorldGenerationModule: started (auto_generate=%d, presets %s, out %s)",
                 mAutoGenerate ? 1 : 0, mPresetsDir.c_str(), mOutPresetsDir.c_str());
    return StartResult::kReady;
}

void WorldGenerationModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    DIA_TRACE_ZONE("WorldGenerationModule::DoUpdate", Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("WorldGenerationModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    if (!mAutoGenerate && !mActionSeen)
        return;

    RandomStreamModule* rng = mRandom.Get();
    Dia::Maths::RandomStream* stream = rng ? rng->GetWorldStream() : nullptr;
    if (!stream)
    {
        mHealth.SetFailing(Dia::Core::StringCRC("wld_stream_unavailable"));
        return;
    }

    if (!mSession.Tick(*stream))
        return;

    mGenerationsCounter->Inc();
    mRevisionGauge->Set(static_cast<double>(mSession.Revision()));
    mLastMsGauge->Set(static_cast<double>(mSession.Generator().GetLastTotalMs()));

    WorldGenerationComplete msg;
    msg.seed = mSession.Seed();
    msg.retryAttempts = 0;
    msg.worldRevision = mSession.Revision();
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (host && host->GetBus().Broadcast(msg))
    {
        mPublishFailed = false;
        mHealth.SetOK();
        DIA_LOG_INFO("CoW", "WorldGenerationModule: revision %u generated (seed %llu)",
                     msg.worldRevision, static_cast<unsigned long long>(msg.seed));
    }
    else
    {
        mPublishDroppedCounter->Inc();
        mHealth.IncrementErrors();
        if (!mPublishFailed)
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: WorldGenerationComplete for revision %u could not be published",
                          msg.worldRevision);
        mPublishFailed = true;
        mHealth.SetFailing(Dia::Core::StringCRC("publish_failed"));
    }
}

void WorldGenerationModule::RegisterCommands()
{
    using Dia::API::ValueParam;
    using Dia::DebugServer::SimHelpInfo;
    using Cluiche::AppFlow::SimQueryHostModule;

    SimQueryHostModule* host = mSimQueryHost.Get();    if (!host)
    {
        DIA_LOG_WARNING("CoW", "WorldGenerationModule: no SimQueryHostModule - cow.worldgen.* commands not registered");
        return;
    }

    static const ValueParam kSetParams[] = {
        { "params", "object", false, "Any WorldShapeParams key (landFraction, continents, rivers, ...), plus seed and unlock" } };
    static const ValueParam kSetSizeParams[] = {
        { "size", "string", true, "World size: Small, Standard or Large" } };
    static const ValueParam kRegenParams[] = {
        { "seed", "int", false, "Optional new seed; the whole pipeline re-runs from scratch" } };
    static const ValueParam kLoadParams[] = {
        { "id", "string", true, "Preset id" },
        { "unlock", "bool", false, "Ignored; accepted for compatibility" } };
    static const ValueParam kSaveParams[] = {
        { "id", "string", true, "Preset id to write into the out presets directory" },
        { "displayName", "string", true, "Human readable preset name" },
        { "description", "string", false, "Preset description" },
        { "overwrite", "bool", false, "Replace an existing preset with the same id" } };
    static const ValueParam kLayersParams[] = {
        { "layer", "string", false, "Layer name for paged values; omit for the stage summary (with per-layer hash)" },
        { "offset", "int", false, "First cell index of the page (default 0)" },
        { "count", "int", false, "Cells in the page (default 500, max 1000; 500 for float layers)" } };
    static const ValueParam kCellParams[] = {
        { "x", "int", false, "Cell column (with y)" },
        { "y", "int", false, "Cell row (with x)" },
        { "index", "int", false, "Linear cell index (alternative to x,y)" } };

    // Handlers always return a JSON object (a null return is a handler failure).
    auto asObject = [](Json::Value v) -> Json::Value
    {
        if (!v.isObject())
        {
            Json::Value e(Json::objectValue);
            e["success"] = false;
            e["error"] = "internal error: non-object result";
            return e;
        }
        return v;
    };

    bool ok = true;
    auto registerOne = [&](bool isAction, const char* name, const char* desc, const ValueParam* params, unsigned count,
                           std::function<Json::Value(const Json::Value&)> fn)
    {
        SimHelpInfo help;
        help.description = desc;
        help.category = "worldgen";
        help.owner = "WorldGenerationModule";
        help.params = params;
        help.paramCount = count;
        help.noArgs = (count == 0);
        bool r = false;
        if (isAction)
        {
            r = host->RegisterCommand(name, help,
                [this, asObject, fn](const Json::Value& args) -> Json::Value
                {
                    Json::Value result = asObject(fn(args));
                    NotifyAction();
                    return result;
                });
        }
        else
        {
            r = host->RegisterQuery(name, help,
                [asObject, fn](const Json::Value& args) -> Json::Value
                {
                    Json::Value result = asObject(fn(args));
                    if (!result.isMember("success")) result["success"] = true;
                    return result;
                });
        }
        if (!r) DIA_LOG_ERROR("CoW", "WorldGenerationModule: %s registration rejected by SimQueryHost", name);
        ok = ok && r;
    };

    WorldGeneration::WorldGenSession* s = &mSession;
    registerOne(true, "cow.worldgen.set", "Set world shape parameters and/or the seed; regenerates on the next sim tick",
                kSetParams, 1, [s](const Json::Value& a) { return s->Set(a); });
    registerOne(true, "cow.worldgen.set_size", "Set the world size (Small, Standard, Large); regenerates on the next sim tick",
                kSetSizeParams, 1, [s](const Json::Value& a) { return s->SetSize(a); });
    registerOne(true, "cow.worldgen.regenerate", "Re-run world generation from scratch, optionally with a new seed",
                kRegenParams, 1, [s](const Json::Value& a)
                {
                    Json::Value r = s->Regenerate(a);
                    if (r.isObject() && r.isMember("success") && r["success"].asBool())
                    {
                        // Generation runs on the next module tick, so report the current revision and last duration.
                        const Json::Value st = s->Status();
                        r["revision"] = st["revision"];
                        r["ms"] = st["lastMs"];
                        r["pending"] = true;
                    }
                    return r;
                });
    registerOne(true, "cow.worldgen.preset.load", "Load a shape preset by id (replaces all shape params, keeps the seed)",
                kLoadParams, 2, [s](const Json::Value& a) { return s->PresetLoad(a); });
    registerOne(true, "cow.worldgen.preset.save", "Save the current shape params as a preset in the writable presets directory",
                kSaveParams, 4, [s](const Json::Value& a) { return s->PresetSave(a); });

    registerOne(false, "cow.worldgen.params", "Current world shape parameters, seed and size", nullptr, 0,
                [s](const Json::Value&) { return s->Params(); });
    registerOne(false, "cow.worldgen.status", "Generation state, revision, seed, dimensions and last generation time", nullptr, 0,
                [s](const Json::Value&) { return s->Status(); });
    registerOne(false, "cow.worldgen.presets", "List the available shape presets", nullptr, 0,
                [s](const Json::Value&)
                {
                    Json::Value r(Json::objectValue);
                    r["success"] = true;
                    r["presets"] = s->Presets();
                    return r;
                });
    registerOne(false, "cow.worldgen.layers", "Terrain layer stage summary with hashes, or paged cell values (offset/count) for one layer", kLayersParams, 3,
                [s](const Json::Value& a) { return s->Layers(a); });
    registerOne(false, "cow.worldgen.cell", "Terrain data for one cell, by x,y or linear index", kCellParams, 3,
                [s](const Json::Value& a) { return s->Cell(a); });

    mCommandsRegistered = ok;
}

void WorldGenerationModule::UnregisterCommands()
{
    if (!mCommandsRegistered) return;
    if (Cluiche::AppFlow::SimQueryHostModule* host = mSimQueryHost.Get())
    {
        for (const char* name : kCommandNames)
            host->UnregisterQuery(name);
    }
    mCommandsRegistered = false;
}

Dia::ApplicationFlow::StopResult WorldGenerationModule::DoStop()
{
    UnregisterCommands();
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
    mGenerationsCounter = mPublishDroppedCounter = nullptr;
    mLastMsGauge = mRevisionGauge = nullptr;
    DIA_LOG_INFO("CoW", "WorldGenerationModule: stopped at revision %u", mSession.Revision());
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using WorldGenerationModule_ = CoW::WorldGenerationModule; }
DIA_MODULE(WorldGenerationModule_);
DIA_DESCRIBE(WorldGenerationModule_::kTypeId, "S-1 terrain world generation session, regenerates on demand.");
