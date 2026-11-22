#include "Modules/WorldGen/WorldGenerationModule.h"

#include <DraoiHexWorld/BiomeRegistry.h>
#include <Shared/CoWNodeKinds.h>
#include <Shared/CoWStepSeedNames.h>
#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/WorldGeneration/GuaranteeSpec.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <functional>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace CoW {

const Dia::Core::StringCRC WorldGenerationModule::kTypeId("WorldGenerationModule");

WorldGenerationModule::WorldGenerationModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    mSession.SetSiteRegistry(&mSiteRegistry);
    mSession.SetStrataAssignerRegistry(&mStrataAssigners);
    mSiteRegistry.SetNodeKinds(WorldGeneration::CoWNodeKindIds());   // CoW's saved built-in node kind ids
    mSession.SetStepSeedNames(WorldGeneration::CoWStepSeedNames());   // CoW's RNG step ids: keeps CoW worlds byte-identical
    mBiomes = std::make_unique<Draoi::HexWorld::BiomeRegistry>();
    mSession.SetBiomeRegistry(mBiomes.get());   // classification reads the module-owned registry; Load swaps its data in place
}

WorldGenerationModule::~WorldGenerationModule()
{
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
}

void WorldGenerationModule::LoadRulesSidecar()
{
    if (mRulesAttempted)
        return;
    mRulesAttempted = true;

    std::string text;
    if (mRulesSidecarTextSet)
    {
        text = mRulesSidecarText;
    }
    else
    {
        // The one sanctioned PathStore read in this module (user-approved exception to the no-statics rule, 2026-11-20,
        // same precedent as Scene2DModule's 'stage_scene'): the catalogue registers every asset id as a PathStore alias.
        // An alias that was explicitly configured never falls back to the default path.
        std::string path = mRulesSidecarPath;
        if (mRulesAliasGiven)
        {
            const Dia::Core::Path::Alias alias(mRulesSidecarAlias.c_str());
            if (!Dia::Core::PathStore::IsPathAliasRegistered(alias))
            {
                mRulesErrorKey = mRulesSidecarAlias;
                mRulesErrorReason = "alias '" + mRulesSidecarAlias + "' is not registered";
                DIA_LOG_ERROR("CoW", "WorldGenerationModule: terrain rules sidecar %s; generation is refused", mRulesErrorReason.c_str());
                return;
            }
            path = Dia::Core::PathStore::ResolvePathToCString(alias);
        }
        std::ifstream in(path, std::ios::binary);
        if (!in.good())
        {
            // The health reason is capped at 64 characters, so it names the alias; the path is in the log and the reason.
            mRulesErrorKey = mRulesAliasGiven ? mRulesSidecarAlias : std::string("sidecar");
            mRulesErrorReason = "cannot read " + path + (mRulesAliasGiven ? " (alias '" + mRulesSidecarAlias + "')" : std::string());
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: terrain rules sidecar %s", mRulesErrorReason.c_str());
            return;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        text = ss.str();
    }

    Dia::Attribute::ParamError err;
    if (!mSession.LoadRules(text.c_str(), err))
    {
        mRulesErrorKey = err.key.AsCStr();
        if (mRulesErrorKey.empty())
            mRulesErrorKey = "sidecar";
        mRulesErrorReason = err.reason.AsCStr();
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: terrain rules sidecar failed at key '%s': %s; generation is refused",
                      mRulesErrorKey.c_str(), mRulesErrorReason.c_str());
    }
}

void WorldGenerationModule::ReportRulesFailure()
{
    char reason[Dia::Core::StringCRC::kStringLength];
    std::snprintf(reason, sizeof(reason), "rules_load_failed:%s", mRulesErrorKey.c_str());
    mHealth.SetFailing(Dia::Core::StringCRC(reason));
    mHealth.IncrementErrors();
}

namespace
{
    bool ReadWholeFile(const std::string& path, std::string& out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.good()) { return false; }
        std::stringstream ss;
        ss << in.rdbuf();
        out = ss.str();
        return true;
    }

    std::string ResolveAlias(const char* alias)
    {
        const Dia::Core::Path::Alias a(alias);
        return Dia::Core::PathStore::IsPathAliasRegistered(a) ? std::string(Dia::Core::PathStore::ResolvePathToCString(a)) : std::string();
    }
}

bool WorldGenerationModule::LoadStrata(Draoi::HexWorld::StratumTable& strata)
{
    std::string strataText;
    std::string sidecarText;
    if (mStrataTextSet)
    {
        strataText = mStrataText;
        sidecarText = mStrataSidecarText;
    }
    else
    {
        std::string strataPath = mStrataPath;
        std::string sidecarPath;
        if (strataPath.empty() && !mBiomesPath.empty())
        {
            strataPath = (std::filesystem::path(mBiomesPath).parent_path() / "strata.json").string();
        }
        if (!strataPath.empty())
        {
            sidecarPath = (std::filesystem::path(strataPath).parent_path() / "strata.schema.json").string();
        }
        else
        {
            strataPath = ResolveAlias(kStrataAlias);
            sidecarPath = ResolveAlias(kStrataSidecarAlias);
            if (strataPath.empty() || sidecarPath.empty())
            {
                mBiomesErrorField = "strata";
                mBiomesErrorReason = std::string("alias '") + (strataPath.empty() ? kStrataAlias : kStrataSidecarAlias) + "' is not registered";
                DIA_LOG_ERROR("CoW", "WorldGenerationModule: strata %s; generation is refused", mBiomesErrorReason.c_str());
                return false;
            }
        }
        if (!ReadWholeFile(strataPath, strataText) || !ReadWholeFile(sidecarPath, sidecarText))
        {
            mBiomesErrorField = "strata";
            mBiomesErrorReason = "cannot read " + strataPath + " or " + sidecarPath;
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: strata %s; generation is refused", mBiomesErrorReason.c_str());
            return false;
        }
    }
    Draoi::HexWorld::StratumError err;
    if (!strata.LoadFromText(strataText.c_str(), sidecarText.c_str(), err, mStrataAssigners.TypeCheck(), mStrataAssigners.MetricCheck()))
    {
        mBiomesErrorField = std::string("strata.") + err.field;
        mBiomesErrorReason = err.reason;
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: strata failed at field '%s': %s; generation is refused",
                      err.field.c_str(), mBiomesErrorReason.c_str());
        return false;
    }
    return true;
}

bool WorldGenerationModule::LoadGuarantees(const Draoi::HexWorld::StratumTable& strata)
{
    std::string text;
    std::string sidecar;
    if (mGuaranteesTextSet)
    {
        text = mGuaranteesText;
        sidecar = mGuaranteesSidecarText;
    }
    else
    {
        std::string path = mGuaranteesPath;
        std::string sidecarPath;
        if (path.empty() && !mBiomesPath.empty())
        {
            const std::filesystem::path beside = std::filesystem::path(mBiomesPath).parent_path() / "guarantees.json";
            if (std::filesystem::exists(beside)) { path = beside.string(); }
        }
        if (!path.empty())
        {
            sidecarPath = (std::filesystem::path(path).parent_path() / "guarantees.schema.json").string();
        }
        else
        {
            path = ResolveAlias(kGuaranteesAlias);
            sidecarPath = ResolveAlias(kGuaranteesSidecarAlias);
            if (path.empty() && sidecarPath.empty())
            {
                return true;   // no source: the spec stays empty
            }
            if (path.empty() || sidecarPath.empty())
            {
                mBiomesErrorField = "guarantees";
                mBiomesErrorReason = std::string("alias '") + (path.empty() ? kGuaranteesAlias : kGuaranteesSidecarAlias) + "' is not registered";
                DIA_LOG_ERROR("CoW", "WorldGenerationModule: guarantees %s; generation is refused", mBiomesErrorReason.c_str());
                return false;
            }
        }
        if (!ReadWholeFile(path, text) || !ReadWholeFile(sidecarPath, sidecar))
        {
            mBiomesErrorField = "guarantees";
            mBiomesErrorReason = "cannot read " + path + " or " + sidecarPath;
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: guarantees %s; generation is refused", mBiomesErrorReason.c_str());
            return false;
        }
    }
    WorldGeneration::GuaranteeSpec spec;
    WorldGeneration::GuaranteeSpecError err;
    if (!WorldGeneration::LoadGuaranteeSpec(text.c_str(), sidecar.c_str(), strata, spec, err))
    {
        mBiomesErrorField = std::string("guarantees.") + err.entry;
        mBiomesErrorReason = err.reason;
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: guarantees failed at '%s': %s; generation is refused",
                      err.entry.c_str(), mBiomesErrorReason.c_str());
        return false;
    }
    mSession.SetGuaranteeSpec(spec);
    // Data-declared kinds without a placer show in the sites overlay; ids a placer already owns are left alone.
    if (!WorldGeneration::InstallSiteKinds(spec, mSiteRegistry, true))
    {
        mBiomesErrorField = "guarantees.site_kinds";
        mBiomesErrorReason = "a declared site kind cannot be installed";
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: guarantees site kinds could not be installed; generation is refused");
        return false;
    }
    return true;
}

void WorldGenerationModule::LoadBiomes()
{
    Draoi::HexWorld::StratumTable strata;
    if (!LoadStrata(strata) || !LoadGuarantees(strata))
    {
        return;
    }
    std::string text;
    std::string path;
    if (mBiomesTextSet)
    {
        text = mBiomesText;
    }
    else
    {
        // Same sanctioned PathStore read as the rules sidecar: the catalogue registers json.biomes as an alias.
        path = mBiomesPath;
        if (path.empty())
        {
            const Dia::Core::Path::Alias alias(kBiomesAlias);
            if (!Dia::Core::PathStore::IsPathAliasRegistered(alias))
            {
                mBiomesErrorField = kBiomesAlias;
                mBiomesErrorReason = std::string("alias '") + kBiomesAlias + "' is not registered";
                DIA_LOG_ERROR("CoW", "WorldGenerationModule: biomes %s; generation is refused", mBiomesErrorReason.c_str());
                return;
            }
            path = Dia::Core::PathStore::ResolvePathToCString(alias);
        }
        std::ifstream in(path, std::ios::binary);
        if (!in.good())
        {
            mBiomesErrorField = "file";
            mBiomesErrorReason = "cannot read " + path;
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: biomes %s; generation is refused", mBiomesErrorReason.c_str());
            return;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        text = ss.str();
    }

    Draoi::HexWorld::RegistryError err;
    if (!mBiomes->LoadFromText(text.c_str(), strata, err, &WorldGeneration::IsBuiltInPinRule))
    {
        mBiomesErrorField = err.field.AsCStr();
        if (mBiomesErrorField.empty())
            mBiomesErrorField = "file";
        mBiomesErrorReason = err.reason.AsCStr();
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: biomes failed at field '%s', id '%s', row %u: %s; generation is refused",
                      mBiomesErrorField.c_str(), err.id.AsCStr(), static_cast<unsigned>(err.row), mBiomesErrorReason.c_str());
        return;
    }
    mBiomesLoaded = true;   // BiomeRegistry::Load logs the Info line itself
}

void WorldGenerationModule::ReportBiomesFailure()
{
    mSession.SetGenerationBlocked("biomes_not_loaded");
    char reason[Dia::Core::StringCRC::kStringLength];
    std::snprintf(reason, sizeof(reason), "biomes_load_failed:%s", mBiomesErrorField.c_str());
    mHealth.SetFailing(Dia::Core::StringCRC(reason));
    mHealth.IncrementErrors();
}

void WorldGenerationModule::OnConfigure(const char* configJson)
{
    // The rules alias is read first. Catalogue aliases are registered by AssetServiceModule::DoStart, after every
    // OnConfigure, so an alias-configured module defers the rules load and the rest of the config to its own DoStart.
    // A host that supplied the sidecar text (tests) has no alias to wait for.
    if (configJson != nullptr && configJson[0] != '\0')
    {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(configJson, root) && root.isObject() && root.isMember("rules_sidecar_alias"))
        {
            if (root["rules_sidecar_alias"].isString() && !root["rules_sidecar_alias"].asString().empty())
            {
                mRulesSidecarAlias = root["rules_sidecar_alias"].asString();
                mRulesAliasGiven = true;
            }
            else
                DIA_LOG_WARNING("CoW", "WorldGenerationModule: rules_sidecar_alias must be a non-empty string, ignored");
        }
    }
    if (mRulesAliasGiven && !mRulesSidecarTextSet)
    {
        mConfigPending = configJson;
        mConfigDeferred = true;
        return;
    }
    ApplyConfig(configJson);
}

void WorldGenerationModule::ApplyConfig(const char* configJson)
{
    LoadRulesSidecar();
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
            if (root.isMember("worlds_dir"))
            {
                if (root["worlds_dir"].isString())
                    mWorldsDir = root["worlds_dir"].asString();
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: worlds_dir must be a string, ignored");
            }
            if (root.isMember("out_worlds_dir"))
            {
                if (root["out_worlds_dir"].isString())
                    mOutWorldsDir = root["out_worlds_dir"].asString();
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: out_worlds_dir must be a string, ignored");
            }
            if (root.isMember("auto_generate"))
            {
                if (root["auto_generate"].isBool())
                    mAutoGenerate = root["auto_generate"].asBool();
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: auto_generate must be a bool, ignored");
            }
            if (root.isMember("regenerate_on_start"))
            {
                // A Regenerate pending from the start: regions and sites are placed by the first generation, which
                // waits for dependants' registrations (see HoldGeneration).
                if (root["regenerate_on_start"].isBool())
                {
                    if (root["regenerate_on_start"].asBool())
                        mSession.Regenerate(Json::Value(Json::objectValue));
                }
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: regenerate_on_start must be a bool, ignored");
            }
            if (root.isMember("max_retry_attempts"))
            {
                const Json::Value& v = root["max_retry_attempts"];
                if (v.isNumeric() && v.isIntegral() && v.asDouble() >= 0.0 && v.asDouble() <= 4.0e9)
                    mSession.SetMaxRetryAttempts(static_cast<uint32_t>(v.asDouble()));
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: max_retry_attempts must be a non-negative integer, ignored");
            }
            if (root.isMember("timeout_seconds"))
            {
                const Json::Value& v = root["timeout_seconds"];
                if (v.isNumeric() && v.asDouble() >= 0.0)
                    mSession.SetTimeoutSeconds(static_cast<float>(v.asDouble() < 1.0e9 ? v.asDouble() : 1.0e9));
                else
                    DIA_LOG_WARNING("CoW", "WorldGenerationModule: timeout_seconds must be a non-negative number, ignored");
            }
            ApplySeedAndSize(root);

            // World hand-off (SD-S1-010): the stage names its world. At most one of preset / world_file / world_bundle.
            const char* const sources[] = { "preset", "world_file", "world_bundle" };
            int named = 0;
            for (const char* key : sources)
                if (root.isMember(key)) ++named;
            auto handoffFail = [this](const std::string& why) { if (mHandoffError.empty()) mHandoffError = why; };
            if (named > 1)
            {
                handoffFail("more than one world source given (preset, world_file and world_bundle are mutually exclusive)");
            }
            else if (root.isMember("world_bundle"))
            {
                // F-3 World Assets is not built (spec only): the bundle form is a seam, not a feature.
                handoffFail("world_bundle is not supported yet: F-3 World Assets is not built");
            }
            else if (root.isMember("preset"))
            {
                if (root["preset"].isString() && !root["preset"].asString().empty())
                    mHandoffPreset = root["preset"].asString();
                else
                    handoffFail("\"preset\" must be a non-empty preset id string");
            }
            else if (root.isMember("world_file"))
            {
                if (root["world_file"].isString() && !root["world_file"].asString().empty())
                {
                    mHandoffWorldFile = root["world_file"].asString();
                    if (root.isMember("seed") || root.isMember("size"))
                        DIA_LOG_WARNING("CoW", "WorldGenerationModule: seed and size are ignored with world_file (the file carries its own)");
                }
                else
                    handoffFail("\"world_file\" must be a non-empty world name string");
            }
        }
    }
    mSession.SetPresetDirectories(mPresetsDir, mOutPresetsDir);
    mSession.SetWorldDirectories(mWorldsDir, mOutWorldsDir);

    ApplyHandoffPreset();
}

void WorldGenerationModule::ApplySeedAndSize(const Json::Value& root)
{
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

void WorldGenerationModule::ApplyHandoffPreset()
{
    // Preset form: the preset's shape with the configured seed and size, then a Regenerate (regions and sites placed).
    // The first generation still waits for the start deferral and any holds, like regenerate_on_start.
    if (mHandoffError.empty() && !mHandoffPreset.empty())
    {
        Json::Value args(Json::objectValue);
        args["id"] = mHandoffPreset;
        const Json::Value loaded = mSession.PresetLoad(args);
        if (loaded.isMember("success") && loaded["success"].asBool())
            mSession.Regenerate(Json::Value(Json::objectValue));
        else
            mHandoffError = "preset \"" + mHandoffPreset + "\" could not be loaded: " + loaded["error"].asString();
    }
}

void WorldGenerationModule::OnStageLaunch(const char* overridesJson)
{
    if (overridesJson == nullptr || overridesJson[0] == '\0')
        return;
    if (mConfigDeferred)
    {
        // The manifest config has not been applied yet (rules load in DoStart): the launch override applies after it.
        mPendingLaunchOverrides = overridesJson;
        return;
    }
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(overridesJson, root) || !root.isObject())
    {
        DIA_LOG_WARNING("CoW", "WorldGenerationModule: launch preset override is not a JSON object, ignored");
        return;
    }
    DIA_LOG_INFO("CoW", "WorldGenerationModule: applying launch preset override (%u bytes)", static_cast<unsigned>(std::strlen(overridesJson)));
    ApplySeedAndSize(root);
    if (root.isMember("preset"))
    {
        if (root["preset"].isString() && !root["preset"].asString().empty())
        {
            mHandoffPreset = root["preset"].asString();
            ApplyHandoffPreset();
        }
        else
        {
            mHandoffError = "launch preset override: \"preset\" must be a non-empty preset id string";
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: %s", mHandoffError.c_str());
        }
    }
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
    mAttemptsCounter       = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.attempts"));
    mUnsatisfiableCounter  = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.unsatisfiable"));
    mTimeoutsCounter       = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.timeouts"));
    mErrorsCounter         = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.errors"));
    mCancelsCounter        = metrics.RegisterCounter(Dia::Core::StringCRC("cow.worldgen.cancels"));
    mLastMsGauge           = metrics.RegisterGauge(Dia::Core::StringCRC("cow.worldgen.last_ms"));
    mRevisionGauge         = metrics.RegisterGauge(Dia::Core::StringCRC("cow.worldgen.revision"));

    // GameSpeedControllerModule also registers this type; RegisterType is an idempotent no-op
    // (returns false) when already registered, so start order does not matter.
    Dia::MessageBus::Bus& bus = host->GetBus();
    bus.RegisterType<WorldGenerationComplete, 4>();
    bus.RegisterProducer<WorldGenerationComplete>(GetInstanceId());
    bus.RegisterType<WorldGenerationRetry, 16>();
    bus.RegisterProducer<WorldGenerationRetry>(GetInstanceId());
    bus.RegisterType<WorldGenerationFailed, 4>();
    bus.RegisterProducer<WorldGenerationFailed>(GetInstanceId());

    mPublishFailed = false;
    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealthRegistered = true;
    mHealth.SetOK();

    RegisterCommands();

    // Done by OnConfigure unless the rules come from a catalogue alias: then the load and the deferred config
    // (seed / size / preset / regenerate_on_start) happen here, followed by any launch override held back.
    LoadRulesSidecar();
    if (mConfigDeferred)
    {
        mConfigDeferred = false;
        const std::string config = std::move(mConfigPending);
        ApplyConfig(config.c_str());
        if (!mPendingLaunchOverrides.empty())
        {
            const std::string overrides = std::move(mPendingLaunchOverrides);
            mPendingLaunchOverrides.clear();
            OnStageLaunch(overrides.c_str());
        }
    }
    if (!mSession.RulesLoaded())
        ReportRulesFailure();

    LoadBiomes();
    if (!mBiomesLoaded)
        ReportBiomesFailure();

    if (!mHandoffError.empty())
        ReportHandoffFailure();

    DIA_LOG_INFO("CoW", "WorldGenerationModule: started (auto_generate=%d, presets %s, out %s)",
                 mAutoGenerate ? 1 : 0, mPresetsDir.c_str(), mOutPresetsDir.c_str());
    return StartResult::kReady;
}

void WorldGenerationModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    DIA_TRACE_ZONE("WorldGenerationModule::DoUpdate", Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("WorldGenerationModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    // Registrations by dependent modules happen in their DoStart: hold the first generation until they are in place.
    if (mStartDeferral)
    {
        mStartDeferral = false;
        return;
    }
    if (mGenerationHolds > 0)
        return;

    // Import the named world file now, not in DoStart: dependants started after this module have subscribed to
    // WorldGenerationComplete by the first real frame, and the import publishes it. Naming a world is an explicit
    // request, so it loads even with auto_generate off.
    if (!mHandoffWorldFile.empty())
        ApplyHandoffWorldFile();

    if ((!mAutoGenerate || mHandoffFailed) && !mActionSeen)
        return;

    RandomStreamModule* rng = mRandom.Get();
    Dia::Maths::RandomStream* stream = rng ? rng->GetWorldStream() : nullptr;
    if (!stream)
    {
        mHealth.SetFailing(Dia::Core::StringCRC("wld_stream_unavailable"));
        return;
    }

    const bool worldChanged = mSession.Tick(*stream);
#ifdef DIA_DEBUG
    // Keep the shared grid in step with the world's dimensions unless a registrant drives rebuilds itself. Also builds
    // the grid on the first frame, so GetHexGrid() is non-null from then on.
    if (!mHexGrid.IsDriven())
        mHexGrid.EnsureDims(mSession.Revision() > 0 ? mSession.World().dimensions : mSession.Dimensions());
#endif

    // A Regenerate just finished (live preview and the first generation never retry): events, counters and health,
    // whether or not it produced a new world (a failed one keeps the previous world and returns false).
    const bool retryRun = mSession.ReportRevision() != mPublishedReportRevision;
    if (retryRun)
    {
        mPublishedReportRevision = mSession.ReportRevision();
        OnRetryRunFinished();
    }
    if (!worldChanged)
        return;

    mGenerationsCounter->Inc();
    mRevisionGauge->Set(static_cast<double>(mSession.Revision()));
    mLastMsGauge->Set(static_cast<double>(mSession.Generator().GetLastTotalMs()));
    const int unresolvedPairs = mSession.Generator().GetEffective().unresolvedPairs;
    if (unresolvedPairs > 0)
    {
        DIA_LOG_INFO("CoW", "WorldGenerationModule: revision %u left %d forbidden biome neighbour pair(s) unresolved",
                     mSession.Revision(), unresolvedPairs);
    }

    if (mSession.State() == WorldGeneration::WorldGenState::Failed)
        return;   // unsatisfiable: no accepted world, so no WorldGenerationComplete

    WorldGenerationComplete msg;
    msg.seed = mSession.Seed();
    msg.retryAttempts = (retryRun && mSession.LastReport()->attempts.Size() > 0)
                            ? static_cast<uint32_t>(mSession.LastReport()->attempts.Size() - 1) : 0u;
    msg.worldRevision = mSession.Revision();
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (host && host->GetBus().Broadcast(msg))
    {
        mPublishFailed = false;
        if (!mGenerationUnsatisfiable)
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

void WorldGenerationModule::OnRetryRunFinished()
{
    const WorldGeneration::GenerationReport* report = mSession.LastReport();
    if (!report)
        return;

    mAttemptsCounter->Inc(report->attempts.Size());
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    for (unsigned i = 0; i < report->attempts.Size(); ++i)
    {
        const WorldGeneration::AttemptRecord& a = report->attempts[i];
        if (a.passed)
            continue;
        WorldGenerationRetry retry;
        retry.attemptNumber = a.attempt + 1;
        retry.maxAttempts = report->maxAttempts;
        retry.failedGuaranteeCount = a.failedGuaranteeCount;
        if (!host || !host->GetBus().Broadcast(retry))
            mPublishDroppedCounter->Inc();   // progress events are best effort; the report keeps the full detail
    }

    using WorldGeneration::GenerationOutcome;
    if (report->outcome == GenerationOutcome::Success)
    {
        mGenerationUnsatisfiable = false;
        return;
    }

    // Ended without a world: tell the dependants why. The previous world and its revision are untouched.
    WorldGenerationFailed failed;
    switch (report->outcome)
    {
    case GenerationOutcome::Timeout:   failed.reason = FailureReason::Timeout; break;
    case GenerationOutcome::Cancelled: failed.reason = FailureReason::ManualCancel; break;
    case GenerationOutcome::Error:     failed.reason = FailureReason::Error; break;
    default:                           failed.reason = FailureReason::Unsatisfiable; break;
    }
    failed.seed = report->outerSeed;
    failed.attempts = static_cast<uint32_t>(report->attempts.Size());
    std::snprintf(failed.message, sizeof(failed.message), "%s", report->failureMessage);   // truncated, always terminated
    if (!host || !host->GetBus().Broadcast(failed))
        mPublishDroppedCounter->Inc();

    if (report->outcome == GenerationOutcome::Cancelled)
    {
        mCancelsCounter->Inc();   // a user action: not a failure, health unchanged
        DIA_LOG_INFO("CoW", "WorldGenerationModule: generation for seed %llu cancelled after %u attempt(s); the previous world is kept",
                     static_cast<unsigned long long>(report->outerSeed), static_cast<unsigned>(report->attempts.Size()));
        return;
    }

    mGenerationUnsatisfiable = true;
    mHealth.IncrementErrors();
    const char* healthReason = "generation_unsatisfiable";
    if (report->outcome == GenerationOutcome::Timeout)
    {
        mTimeoutsCounter->Inc();
        healthReason = "generation_timeout";
    }
    else if (report->outcome == GenerationOutcome::Error)
    {
        mErrorsCounter->Inc();
        healthReason = "generation_error";
    }
    else
    {
        mUnsatisfiableCounter->Inc();
    }
    mHealth.SetFailing(Dia::Core::StringCRC(healthReason));
    DIA_LOG_ERROR("CoW", "WorldGenerationModule: generation for seed %llu ended %s after %u attempt(s) (%s); see cow.worldgen.report",
                  static_cast<unsigned long long>(report->outerSeed), WorldGeneration::GenerationOutcomeName(report->outcome),
                  static_cast<unsigned>(report->attempts.Size()), report->failureMessage);
}

void WorldGenerationModule::LogWorldFileReply(const char* action, const Json::Value& reply) const
{
    if (!reply.isObject())
        return;
    if (!reply.isMember("success") || !reply["success"].asBool())
    {
        DIA_LOG_WARNING("CoW", "WorldGenerationModule: world %s failed: %s", action, reply["error"].asCString());
        return;
    }
    DIA_LOG_INFO("CoW", "WorldGenerationModule: world %s ok: %s (%llu bytes)", action, reply["path"].asCString(),
                 static_cast<unsigned long long>(reply["bytes"].asUInt64()));
    const Json::Value& warnings = reply["warnings"];
    for (Json::ArrayIndex i = 0; warnings.isArray() && i < warnings.size() && i < 20; ++i)
        DIA_LOG_INFO("CoW", "WorldGenerationModule: world %s: %s", action, warnings[i].asCString());
}

void WorldGenerationModule::ReportHandoffFailure()
{
    // The hand-off named a world that cannot be loaded: say so loudly, stay started, and do not substitute a
    // different world (no automatic generation until an explicit action).
    DIA_LOG_ERROR("CoW", "WorldGenerationModule: world hand-off failed: %s", mHandoffError.c_str());
    mHandoffFailed = true;
    mHealth.SetFailing(Dia::Core::StringCRC("world_handoff_failed"));
    mHealth.IncrementErrors();
}

void WorldGenerationModule::ApplyHandoffWorldFile()
{
    Json::Value args(Json::objectValue);
    args["name"] = mHandoffWorldFile;
    const Json::Value reply = mSession.ImportWorld(args);
    LogWorldFileReply("import", reply);
    const std::string name = mHandoffWorldFile;
    mHandoffWorldFile.clear();   // once
    if (reply.isObject() && reply.isMember("success") && reply["success"].asBool())
    {
        OnWorldImported();
        return;
    }
    mHandoffError = "world file \"" + name + "\" could not be imported: " +
                    (reply.isObject() && reply["error"].isString() ? reply["error"].asString() : std::string("unknown error"));
    ReportHandoffFailure();
}

Json::Value WorldGenerationModule::ExportWorldFile(const Json::Value& args)
{
    Json::Value r = mSession.ExportWorld(args);
    LogWorldFileReply("export", r);
    return r;
}

Json::Value WorldGenerationModule::ImportWorldFile(const Json::Value& args)
{
    Json::Value r = mSession.ImportWorld(args);
    LogWorldFileReply("import", r);
    if (r.isObject() && r.isMember("success") && r["success"].asBool())
        OnWorldImported();
    return r;
}

void WorldGenerationModule::OnWorldImported()
{
    // The world changed without a generation: keep the gauges and health honest and tell the dependants.
    if (mRevisionGauge)
        mRevisionGauge->Set(static_cast<double>(mSession.Revision()));
    mGenerationUnsatisfiable = false;
    mHealth.SetOK();

    WorldGenerationComplete msg;
    msg.seed = mSession.Seed();
    msg.retryAttempts = 0;
    msg.worldRevision = mSession.Revision();
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (host && host->GetBus().Broadcast(msg))
    {
        DIA_LOG_INFO("CoW", "WorldGenerationModule: revision %u imported (seed %llu), world is locked",
                     msg.worldRevision, static_cast<unsigned long long>(msg.seed));
    }
    else
    {
        if (mPublishDroppedCounter)
            mPublishDroppedCounter->Inc();
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: WorldGenerationComplete for imported revision %u could not be published", msg.worldRevision);
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
        { "params", "object", false, "Any WorldShapeParams key (landFraction, continents, rivers, ...), plus seed and unlock (true is required on a locked imported world; it drops the imported edits)" } };
    static const ValueParam kSetSizeParams[] = {
        { "size", "string", true, "World size: Small, Standard or Large" },
        { "unlock", "bool", false, "Required (true) on a locked imported world; drops the imported edits" } };
    static const ValueParam kRegenParams[] = {
        { "seed", "int", false, "Optional new seed; the whole pipeline re-runs from scratch" },
        { "maxRetryAttempts", "int", false, "Retry bound for regions and sites (default 10, clamped to 1..64); sticks for later regenerates" },
        { "timeoutSeconds", "float", false, "Wall-clock budget for the whole generation, all attempts (default 30, clamped to 0..3600); sticks for later regenerates" },
        { "unlock", "bool", false, "Required (true) on a locked imported world; drops the imported edits" } };
    static const ValueParam kLoadParams[] = {
        { "id", "string", true, "Preset id" },
        { "unlock", "bool", false, "Required (true) on a locked imported world; drops the imported edits" } };
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
    // Names this call actually registered, so a partial failure rolls back only our own (never a duplicate's owner's).
    const char* registeredNames[kCommandCount] = {};
    unsigned registeredCount = 0;
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
        if (r)
        {
            if (registeredCount < kCommandCount) registeredNames[registeredCount++] = name;
        }
        else
        {
            DIA_LOG_ERROR("CoW", "WorldGenerationModule: %s registration rejected by SimQueryHost", name);
        }
        ok = ok && r;
    };

    static const ValueParam kExportParams[] = {
        { "name", "string", false, "World file name, [A-Za-z0-9_-]{1,64}; default the seed. Written to the out worlds directory as <name>.wld.json" },
        { "overwrite", "bool", false, "Replace an existing file with the same name" } };
    static const ValueParam kImportParams[] = {
        { "name", "string", true, "World file name (out worlds directory first, then the deployed one). The world opens locked" } };
    static const ValueParam kReportParams[] = {
        { "offset", "int", false, "First attempt of the page (default 0)" },
        { "limit", "int", false, "Attempts in the page (default and max 12)" },
        { "guarantee", "string", false, "Only this guarantee in finalValidation (a guarantee id as listed by validate, or its numeric id)" },
        { "evidenceOffset", "int", false, "First evidence item (proof path, violating cell or pair) of each guarantee's page (default 0)" },
        { "evidenceLimit", "int", false, "Evidence items per list in the page (default 2 paths, 32 cells, 16 pairs; max 8, 128, 64)" } };
    static const ValueParam kSitesParams[] = {
        { "kind", "string", false, "Only sites of this kind id (e.g. CoW.Node.Corridor or a registered kind)" },
        { "offset", "int", false, "First site of the page (default 0)" },
        { "limit", "int", false, "Sites in the page (default and max 80)" } };

    WorldGeneration::WorldGenSession* s = &mSession;
    registerOne(true, "cow.worldgen.set", "Set world shape parameters and/or the seed; regenerates on the next sim tick",
                kSetParams, 1, [s](const Json::Value& a) { return s->Set(a); });
    registerOne(true, "cow.worldgen.set_size", "Set the world size (Small, Standard, Large); regenerates on the next sim tick",
                kSetSizeParams, 2, [s](const Json::Value& a) { return s->SetSize(a); });
    registerOne(true, "cow.worldgen.regenerate", "Re-run world generation from scratch, optionally with a new seed",
                kRegenParams, 4, [s](const Json::Value& a)
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
    registerOne(true, "cow.worldgen.cancel", "Cancel the Regenerate in progress (or pending): the previous world is kept; nothing to cancel is not an error",
                nullptr, 0, [s](const Json::Value& a) { return s->Cancel(a); });
    registerOne(true, "cow.worldgen.preset.load", "Load a shape preset by id (replaces all shape params, keeps the seed)",
                kLoadParams, 2, [s](const Json::Value& a) { return s->PresetLoad(a); });
    registerOne(true, "cow.worldgen.preset.save", "Save the current shape params as a preset in the writable presets directory",
                kSaveParams, 4, [s](const Json::Value& a) { return s->PresetSave(a); });

    registerOne(true, "cow.worldgen.export", "Write the current world (terrain, biomes, regions, sites, provenance) to a .wld.json file",
                kExportParams, 2, [this](const Json::Value& a) { return ExportWorldFile(a); });
    registerOne(true, "cow.worldgen.import", "Replace the world with one read from a .wld.json file; the world opens locked (set/regenerate need unlock:true)",
                kImportParams, 1, [this](const Json::Value& a) { return ImportWorldFile(a); });

    registerOne(true, "cow.worldgen.validate", "Run the validation guarantees on the current world without changing it; the result is kept as fresh evidence for report and the evidence overlay",
                nullptr, 0, [s](const Json::Value& a) { return s->Validate(a); });

    registerOne(false, "cow.worldgen.worlds", "List the saved worlds: the out and deployed worlds directories merged (out wins on a name clash)", nullptr, 0,
                [s](const Json::Value&) { return s->Worlds(); });
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
    registerOne(false, "cow.worldgen.regions", "Every region record (zone, landmass, biome, purity, cells, neighbours) of the generated world", nullptr, 0,
                [s](const Json::Value&) { return s->Regions(); });
    registerOne(false, "cow.worldgen.sites", "Paged sites (built-in nodes and registered kinds) with the kind table, optionally of one kind", kSitesParams, 3,
                [s](const Json::Value& a) { return s->Sites(a); });
    registerOne(false, "cow.worldgen.report", "The last generation report: outcome, per-attempt sub-seed, failed guarantees and diagnostics (paged)", kReportParams, 5,
                [s](const Json::Value& a) { return s->Report(a); });

    if (!ok)
    {
        // All or nothing: a failed registration must not leave the others in the always-on SimQueryHost, because
        // UnregisterCommands is skipped when mCommandsRegistered is false and the names would leak for good.
        for (unsigned i = 0; i < registeredCount; ++i)
            host->UnregisterQuery(registeredNames[i]);
        DIA_LOG_ERROR("CoW", "WorldGenerationModule: rolled back %u cow.worldgen.* registrations after a failure", registeredCount);
    }
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
#ifdef DIA_DEBUG
    mHexGrid.reset();
#endif
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
    mGenerationsCounter = mPublishDroppedCounter = mAttemptsCounter = mUnsatisfiableCounter = nullptr;
    mTimeoutsCounter = mErrorsCounter = mCancelsCounter = nullptr;
    mLastMsGauge = mRevisionGauge = nullptr;
    DIA_LOG_INFO("CoW", "WorldGenerationModule: stopped at revision %u", mSession.Revision());
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using WorldGenerationModule_ = CoW::WorldGenerationModule; }
DIA_MODULE(WorldGenerationModule_);
DIA_DESCRIBE(WorldGenerationModule_::kTypeId, "S-1 terrain world generation session, regenerates on demand.");
