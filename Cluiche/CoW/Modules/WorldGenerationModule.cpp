#include "Modules/WorldGenerationModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
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

Dia::ApplicationFlow::StopResult WorldGenerationModule::DoStop()
{
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
