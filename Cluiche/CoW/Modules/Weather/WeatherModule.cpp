#include "Modules/Weather/WeatherModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <Modules/Weather/WeatherMessages.h>
#include <Shared/Weather/WeatherTerrainBuilder.h>
#include <Shared/World/WorldSimNames.h>
#include <DiaCore/FilePath/PathStore.h>

#include <fstream>
#include <sstream>

namespace CoW {

// The production seams over WorldGenerationModule: the world read (copied into a WeatherTerrain, never retained) and
// the generation hold.
// The shared step launcher, fetched fresh on every call (the module may swap it while idle).
class WeatherModule::LauncherAdapter : public Weather::ILauncherSource
{
public:
    explicit LauncherAdapter(Dia::ApplicationFlow::ModuleRef<Dia::DeferredStep::DeferredStepModule>& ref) : mRef(ref) {}
    Dia::DeferredStep::IStepLauncher* Get() override
    {
        Dia::DeferredStep::DeferredStepModule* host = mRef.Get();
        return host != nullptr ? host->GetLauncher() : nullptr;
    }

private:
    Dia::ApplicationFlow::ModuleRef<Dia::DeferredStep::DeferredStepModule>& mRef;
};

class WeatherModule::WorldGenAdapter : public Weather::IWeatherWorldSource, public WorldStep::IGenerationHold
{
public:
    explicit WorldGenAdapter(WorldGenerationModule& worldGen) : mWorldGen(worldGen) {}

    bool GetWorld(WorldStep::WorldView& view, Draoi::HexWorld::Weather::WeatherTerrain& terrain) override
    {
        const Draoi::HexWorld::WorldStructure* world = mWorldGen.GetCurrentWorld();
        // A newer world than the announced one exists: its own event follows, so skip this one.
        if (world == nullptr || mWorldGen.GetWorldRevision() != view.revision) { return false; }
        if (!Weather::BuildWeatherTerrain(*world, terrain)) { return false; }
        view.present = true;
        view.width = terrain.width;
        view.height = terrain.height;
        return true;
    }
    void Hold() override { mWorldGen.HoldGeneration(); }
    void Release() override { mWorldGen.ReleaseGeneration(); }

private:
    WorldGenerationModule& mWorldGen;
};

namespace {

// `ref` is a catalogue alias (registered by AssetServiceModule::DoStart, after every OnConfigure) or a literal path.
// The one sanctioned PathStore read, same precedent as WorldGenerationModule's rules sidecar. Resolved at read time.
std::string ResolveRef(const std::string& ref)
{
    const Dia::Core::Path::Alias alias(ref.c_str());
    if (Dia::Core::PathStore::IsPathAliasRegistered(alias))
        return Dia::Core::PathStore::ResolvePathToCString(alias);
    return ref;
}

bool ReadRef(const std::string& ref, std::string& text, std::string& error)
{
    const std::string path = ResolveRef(ref);
    std::ifstream in(path, std::ios::binary);
    if (!in.good())
    {
        error = "cannot read " + path + " ('" + ref + "')";
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
}

class RefSidecarSource : public Weather::IWeatherSidecarSource
{
public:
    explicit RefSidecarSource(const std::string& ref) : mRef(ref) {}
    bool Read(std::string& text, std::string& error) override { return ReadRef(mRef, text, error); }
private:
    std::string mRef;
};

} // namespace

const Dia::Core::StringCRC WeatherModule::kTypeId("WeatherModule");

WeatherModule::WeatherModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    mSaveParticipant = std::make_unique<Weather::WeatherSaveParticipant>(mStage);
}

WeatherModule::~WeatherModule() {}

Dia::Core::StringCRC WeatherModule::StageId() const
{
    return Dia::Core::StringCRC(World::StageId(World::kWeather).c_str());
}

void WeatherModule::OnConfigure(const char* configJson)
{
    mConfigOk = true;
    mConfigErrorKey.clear();
    mConfigErrorReason.clear();
    mConfigPacks.clear();
    mClimateIgnored = false;
    mSidecarGiven = false;
    mDevReloadGiven = false;
    mSidecarRef = "json.weather_parameters_schema";
    mSpinupWeeks = Weather::kDefaultSpinupWeeks;

    auto fail = [this](const char* key, const char* reason) {
        mConfigOk = false;
        mConfigErrorKey = key;
        mConfigErrorReason = reason;
        DIA_LOG_ERROR(Weather::kLogChannel, "WTH: WeatherModule config error at '%s': %s; the module will not start", key, reason);
    };

    if (configJson == nullptr || configJson[0] == '\0') { mStage.SetSpinupWeeks(mSpinupWeeks); return; }
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(configJson, root) || !root.isObject()) { fail("config", "not a JSON object"); return; }

    if (root.isMember("spinup_weeks"))
    {
        const Json::Value& v = root["spinup_weeks"];
        const bool numeric = v.isNumeric();
        const double d = numeric ? v.asDouble() : 0.0;
        if (!numeric || d < 0.0 || d > 520.0 || d != static_cast<double>(static_cast<long long>(d)))
        {
            fail("spinup_weeks", "must be an integer in [0, 520]");
            return;
        }
        mSpinupWeeks = static_cast<uint32_t>(d);
    }
    if (root.isMember("sidecar"))
    {
        if (!root["sidecar"].isString() || root["sidecar"].asString().empty())
        {
            fail("sidecar", "must be a non-empty string (catalogue alias or path)");
            return;
        }
        mSidecarRef = root["sidecar"].asString();
        mSidecarGiven = true;
    }
    if (root.isMember("dev_reload"))
    {
        if (!root["dev_reload"].isBool()) { fail("dev_reload", "must be a boolean"); return; }
        mDevReload = root["dev_reload"].asBool();
        mDevReloadGiven = true;
    }
    if (root.isMember("packs"))
    {
        const Json::Value& packs = root["packs"];
        if (!packs.isArray()) { fail("packs", "must be an array"); return; }
        for (Json::ArrayIndex i = 0; i < packs.size(); ++i)
        {
            if (!packs[i].isObject() && !(packs[i].isString() && !packs[i].asString().empty()))
            {
                fail("packs", "each pack must be an object or a non-empty alias/path string");
                return;
            }
            mConfigPacks.push_back(packs[i]);
        }
    }
    if (root.isMember("climate"))
    {
        mClimateIgnored = true;
        DIA_LOG_INFO(Weather::kLogChannel, "WTH: WeatherModule 'climate' config accepted and ignored (climate cycles are not built)");
    }
    mStage.SetSpinupWeeks(mSpinupWeeks);
}

bool WeatherModule::BuildActivePacks()
{
    mActivePacks = mPacks;
    for (const Json::Value& pack : mConfigPacks)
    {
        if (pack.isObject()) { mActivePacks.push_back(pack); continue; }
        std::string text, error;
        Json::Value parsed;
        Json::Reader reader;
        if (!ReadRef(pack.asString(), text, error))
        {
            // error already set
        }
        else if (!reader.parse(text, parsed) || !parsed.isObject())
        {
            error = "pack '" + pack.asString() + "' is not a JSON object";
        }
        if (!error.empty())
        {
            DIA_LOG_ERROR(Weather::kLogChannel, "WTH: WeatherModule parameter pack failed: %s; stage is Failing", error.c_str());
            mStage.FailStage("packs", static_cast<uint32_t>(Weather::CauseCode::kParameterFault));
            return false;
        }
        mActivePacks.push_back(parsed);
    }
    return true;
}

Dia::ApplicationFlow::StartResult WeatherModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    DIA_TRACE_ZONE("WeatherModule::DoStart", Dia::Observation::Trace::Category::kNone);

    if (!mConfigOk)
    {
        DIA_LOG_ERROR(Weather::kLogChannel, "WTH: WeatherModule not starting, config error at '%s': %s",
                      mConfigErrorKey.c_str(), mConfigErrorReason.c_str());
        return StartResult::kFailed;
    }
    WorldStepModule* worldStep = nullptr;
    Dia::MessageBus::Bus* bus = nullptr;
    if (mUseTestHosts)
    {
        worldStep = mTestHosts.worldStep;
        bus = mTestHosts.bus;
        mStage.SetWorldSource(mTestHosts.worldSource);
        mStage.SetGenerationHold(mTestHosts.generationHold);
    }
    else
    {
        worldStep = mWorldStepRef.Get();
        Cluiche::AppFlow::MessageBusHostModule* busHost = mBusRef.Get();
        WorldGenerationModule* worldGen = mWorldGenRef.Get();
        if (busHost == nullptr || worldGen == nullptr)
            return StartResult::kLoading;
        bus = &busHost->GetBus();
        mWorldGen = worldGen;
        mAdapter = std::make_unique<WorldGenAdapter>(*worldGen);
        mStage.SetWorldSource(mAdapter.get());
        mStage.SetGenerationHold(mAdapter.get());
    }
    if (worldStep == nullptr)
        return StartResult::kLoading;

    // The production sidecar source, created from config unless a source was injected. A test-hosts run without an
    // explicit `sidecar` keeps the parameters the stage was handed.
    if (mSidecarSource == nullptr && (!mUseTestHosts || mSidecarGiven))
    {
        mOwnedSource = std::make_unique<RefSidecarSource>(mSidecarRef);
        mSidecarSource = mOwnedSource.get();
    }
    if (mSidecarSource != nullptr)
    {
        DIA_LOG_INFO(Weather::kLogChannel, "WTH: WeatherModule parameter source '%s', %u config pack(s), spin-up %u weeks",
                     mOwnedSource ? mSidecarRef.c_str() : "<injected>", static_cast<unsigned>(mConfigPacks.size()),
                     static_cast<unsigned>(mSpinupWeeks));
    }
    mStage.Start();   // takes the generation hold
    if (!mUseTestHosts)
    {
        // Spin-up runs through the DeferredStep launcher (the same strict worker cap as the weekly step) when
        // DeferredStepModule resolves; otherwise on the JobSystemService pool (tag: the 'world' channel); with no pool
        // either, or a refusing one, the stage runs it inline with one Warning.
        if (mDeferredRef.Get() != nullptr)
        {
            mLauncherAdapter = std::make_unique<LauncherAdapter>(mDeferredRef);
            mStage.SetLauncherSource(mLauncherAdapter.get());
            mStage.SetLaunchMode(Weather::WeatherStage::LaunchMode::kCappedLauncher);
        }
        else
        {
            mStage.SetLaunchMode(Weather::WeatherStage::LaunchMode::kJobSystem);
        }
        mStage.SetJobSubmitter([this](std::function<void()> fn) {
            if (!mJobsReader.IsConnected() || !mJobsReader.IsAvailable()) { return false; }
            Dia::Threading::JobTag tag;
            tag.owner = WorldStepModule::kChannelId.Value();
            tag.tag = Dia::Core::StringCRC("weather").Value();
            return mJobsReader.Get().TrySubmit(std::move(fn), tag).IsValid();
        });
    }
    StartSidecarWatch();
    if (!mUseTestHosts && mWorldGen != nullptr && mWorldGen->AreBiomesLoaded())
        mStage.SetRegistry(&mWorldGen->GetBiomeRegistry());
    TryLoadParameters();
    if (!worldStep->RegisterStage(StageId(), mStage))
    {
        DIA_LOG_ERROR(Weather::kLogChannel, "WeatherModule: WorldStepModule refused stage id '%s'",
                      World::StageId(World::kWeather).c_str());
        mStage.Stop();
        return StartResult::kFailed;
    }
    mWorldStep = worldStep;
    mRegistered = true;

    if (bus != nullptr)
    {
        bus->RegisterType<WorldGenerationComplete, 4>();
        bus->RegisterType<WorldGenerationFailed, 4>();
        bus->RegisterType<WeatherUpdated, 4>();
        mStage.SetPublishSeam([bus](uint64_t week, Draoi::HexWorld::Weather::Season season) {
            WeatherUpdated msg;
            msg.week = week;
            msg.season = season;
            bus->Broadcast(msg);
        });
        mSubscriptions[0] = bus->Subscribe<WorldGenerationComplete>(
            GetInstanceId(), [this](const WorldGenerationComplete& msg) { OnWorldComplete(msg); });
        mSubscriptions[1] = bus->Subscribe<WorldGenerationFailed>(
            GetInstanceId(), [this](const WorldGenerationFailed&) { OnWorldFailed(); });
        if (!mSubscriptions[0].IsValid() || !mSubscriptions[1].IsValid())
        {
            DIA_LOG_ERROR(Weather::kLogChannel, "WeatherModule: a bus subscription failed (handler pool full?)");
        }
    }
    mStage.NotifySubscribed();   // generation may now announce worlds: we are listening
    if (mWorldGen != nullptr && mWorldGen->GetWorldRevision() > 0)
    {
        // A world already exists (imported before we started): ingest it now.
        if (mWorldGen->AreBiomesLoaded()) { mStage.SetRegistry(&mWorldGen->GetBiomeRegistry()); }
        TryLoadParameters();
        mStage.OnWorldComplete(mWorldGen->GetWorldRevision());
    }
    DIA_LOG_INFO(Weather::kLogChannel, "WeatherModule: stage '%s' registered", World::StageId(World::kWeather).c_str());
    return StartResult::kReady;
}

// Dev builds (not COW_SHIPPING_BUILD, the ParamCommandHelper precedent), or config `dev_reload`: watch the configured
// sidecar file; a change queues a Reload, which still applies at Capture. Test-host runs watch only when asked.
void WeatherModule::StartSidecarWatch()
{
    bool enable = mDevReloadGiven ? mDevReload : false;
#ifndef COW_SHIPPING_BUILD
    if (!mDevReloadGiven && !mUseTestHosts) { enable = true; }
#endif
    mWatcher.Stop();
    if (!enable || !mOwnedSource) { return; }
    mWatcher.Start();
    const std::string path = ResolveRef(mSidecarRef);
    mWatcher.Watch(path.c_str(), [this](const char* changed, Dia::Core::FileWatchEvent) {
        DIA_LOG_INFO(Weather::kLogChannel, "WTH: sidecar '%s' changed, reload queued", changed);
        mStage.QueueReload();
    });
}

void WeatherModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    mWatcher.Update();
    mStage.Update();
}

void WeatherModule::OnWorldComplete(const WorldGenerationComplete& msg)
{
    if (mWorldGen != nullptr && mWorldGen->AreBiomesLoaded()) { mStage.SetRegistry(&mWorldGen->GetBiomeRegistry()); }
    TryLoadParameters();
    mStage.OnWorldComplete(msg.worldRevision);
}

// Loads the sidecar once per start. Without a configured source the stage keeps the parameters it was handed (tests).
// A fault fails the stage (Failing naming the dotted key, never ready); the module itself still starts.
void WeatherModule::TryLoadParameters()
{
    if (mParamsAttempted || mSidecarSource == nullptr || !mStage.HasRegistry()) { return; }
    mParamsAttempted = true;
    if (!BuildActivePacks()) { return; }
    Weather::ParamError err;
    mStage.LoadParameters(*mSidecarSource, mActivePacks, err);
}

void WeatherModule::OnWorldFailed()
{
    mStage.OnWorldFailed();
}

void WeatherModule::Unsubscribe()
{
    for (auto& handle : mSubscriptions)
        handle = Dia::MessageBus::BusSubscriptionHandle();
}

Dia::ApplicationFlow::StopResult WeatherModule::DoStop()
{
    DIA_TRACE_ZONE("WeatherModule::DoStop", Dia::Observation::Trace::Category::kNone);
    if (mRegistered && mWorldStep != nullptr)
        mWorldStep->UnregisterStage(StageId());
    mWatcher.Stop();
    mRegistered = false;
    mParamsAttempted = false;
    mWorldStep = nullptr;
    Unsubscribe();
    mStage.SetPublishSeam(nullptr);
    mStage.Stop();   // joins any capped spin-up before the launcher adapter goes
    mStage.SetLauncherSource(nullptr);
    mLauncherAdapter.reset();
    mStage.SetWorldSource(nullptr);
    mStage.SetGenerationHold(nullptr);
    mWorldGen = nullptr;
    mAdapter.reset();
    if (mOwnedSource) { mSidecarSource = nullptr; mOwnedSource.reset(); }
    mActivePacks.clear();
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using WeatherModule_ = CoW::WeatherModule; }
DIA_MODULE(WeatherModule_);
DIA_DESCRIBE(WeatherModule_::kTypeId, "A4 weather: the world-step weather stage host.");
