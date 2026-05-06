#include "DiaAssetRuntime/AssetRuntimeDebugCommands.h"
#include "DiaAssetRuntime/AssetRuntime.h"
#include "DiaAssetRuntime/AssetState.h"
#include "DiaAssetRuntime/AssetScope.h"
#include "DiaAssetRuntime/RuntimeAssetEntry.h"
#include "DiaAssetRuntime/IAssetStateListener.h"

#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaLogger/DiaLog.h>

#include <chrono>

namespace
{
    static Dia::AssetRuntime::TransitionNotifyCallback sNotifyCallback;

    const char* StateToString(Dia::AssetRuntime::AssetState state)
    {
        switch (state)
        {
            case Dia::AssetRuntime::AssetState::Registered: return "Registered";
            case Dia::AssetRuntime::AssetState::Staged:     return "Staged";
            case Dia::AssetRuntime::AssetState::Loaded:     return "Loaded";
            case Dia::AssetRuntime::AssetState::Unloading:  return "Unloading";
            default:                                         return "Unknown";
        }
    }

    const char* ScopeToString(Dia::AssetRuntime::AssetScope scope)
    {
        return (scope == Dia::AssetRuntime::AssetScope::kGlobal) ? "Global" : "Stage";
    }

    // Internal listener that logs state transitions as DiaLogger messages
    // and pushes events via DiaDebugServer when a notify callback is provided.
    class TransitionLogger : public Dia::AssetRuntime::IAssetStateListener
    {
    public:
        void OnAssetReady(const Dia::Core::StringCRC& assetId,
                          const Dia::Core::Containers::String512& resolvedPath) override
        {
            DIA_LOG_INFO("AssetRuntimeDebugCommands",
                "subscribe_transitions: asset '%s' -> Loaded (path: %s)",
                assetId.AsChar(), resolvedPath.AsCStr());

            if (sNotifyCallback)
            {
                Json::Value payload;
                payload["assetId"] = assetId.AsChar();
                payload["oldState"] = "Staged";
                payload["newState"] = "Loaded";
                auto now = std::chrono::system_clock::now();
                payload["timestamp"] = static_cast<Json::UInt64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count());
                sNotifyCallback(Dia::Core::StringCRC("asset_runtime.transitions"), payload);
            }
        }

        void OnAssetUnloading(const Dia::Core::StringCRC& assetId) override
        {
            DIA_LOG_INFO("AssetRuntimeDebugCommands",
                "subscribe_transitions: asset '%s' -> Unloading",
                assetId.AsChar());

            if (sNotifyCallback)
            {
                Json::Value payload;
                payload["assetId"] = assetId.AsChar();
                payload["oldState"] = "Loaded";
                payload["newState"] = "Unloading";
                auto now = std::chrono::system_clock::now();
                payload["timestamp"] = static_cast<Json::UInt64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count());
                sNotifyCallback(Dia::Core::StringCRC("asset_runtime.transitions"), payload);
            }
        }

        void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) override
        {
            DIA_LOG_WARNING("AssetRuntimeDebugCommands",
                "subscribe_transitions: asset '%s' load failed -> Registered",
                assetId.AsChar());

            if (sNotifyCallback)
            {
                Json::Value payload;
                payload["assetId"] = assetId.AsChar();
                payload["oldState"] = "Staged";
                payload["newState"] = "Registered";
                auto now = std::chrono::system_clock::now();
                payload["timestamp"] = static_cast<Json::UInt64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count());
                sNotifyCallback(Dia::Core::StringCRC("asset_runtime.transitions"), payload);
            }
        }
    };

    static TransitionLogger sTransitionLogger;
    static bool             sTransitionLoggerRegistered = false;

} // anonymous namespace

namespace Dia
{
    namespace AssetRuntime
    {
        void RegisterAssetRuntimeCommands(AssetRuntime& runtime, TransitionNotifyCallback notifyCallback)
        {
            sNotifyCallback = notifyCallback;

            // ----------------------------------------------------------------
            // asset_runtime.get-loaded
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-get-loaded");
                cmd.description = "List all assets currently in Loaded state";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-get-loaded";
                cmd.callback    = [&runtime](const Dia::API::CommandArgs&) -> int
                {
                    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> results;
                    unsigned int total = runtime.GetLoadedAssets(results);

                    Json::Value root;
                    root["total"] = total;
                    Json::Value& assets = root["assets"];
                    assets = Json::Value(Json::arrayValue);
                    for (unsigned int i = 0; i < results.Size(); ++i)
                        assets.append(results[i].AsChar());

                    Json::FastWriter writer;
                    DIA_LOG_INFO("AssetRuntimeDebugCommands", "%s", writer.write(root).c_str());
                    return 0;
                };
                Dia::API::RegisterCommand(cmd);
            }

            // ----------------------------------------------------------------
            // asset_runtime.get-staged
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-get-staged");
                cmd.description = "List all assets currently in Staged state";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-get-staged";
                cmd.callback    = [&runtime](const Dia::API::CommandArgs&) -> int
                {
                    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> results;
                    unsigned int total = runtime.GetStagedAssets(results);

                    Json::Value root;
                    root["total"] = total;
                    Json::Value& assets = root["assets"];
                    assets = Json::Value(Json::arrayValue);
                    for (unsigned int i = 0; i < results.Size(); ++i)
                        assets.append(results[i].AsChar());

                    Json::FastWriter writer;
                    DIA_LOG_INFO("AssetRuntimeDebugCommands", "%s", writer.write(root).c_str());
                    return 0;
                };
                Dia::API::RegisterCommand(cmd);
            }

            // ----------------------------------------------------------------
            // asset_runtime.get-state  --assetId=<id>
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-get-state");
                cmd.description = "Get state, scope, ref count and path for a specific asset";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-get-state --assetId=texture.player";
                cmd.callback    = [&runtime](const Dia::API::CommandArgs& args) -> int
                {
                    auto it = args.namedArgs.find(Dia::Core::StringCRC("assetId").Value());
                    if (it == args.namedArgs.end() || !it->second)
                    {
                        DIA_LOG_WARNING("AssetRuntimeDebugCommands",
                            "asset-runtime-get-state: missing --assetId parameter");
                        return 1;
                    }

                    Dia::Core::StringCRC assetId(it->second);
                    AssetState state = runtime.GetAssetState(assetId);
                    unsigned int refCount = runtime.GetAssetRefCount(assetId);
                    const Dia::Core::Containers::String512* path = runtime.ResolveAssetPath(assetId);

                    Json::Value root;
                    root["assetId"]  = it->second;
                    root["state"]    = StateToString(state);
                    root["scope"]    = ScopeToString(runtime.GetAssetScope(assetId));
                    root["refCount"] = refCount;
                    root["deployPath"] = path ? path->AsCStr() : "";

                    AssetScope assetScope = runtime.GetAssetScope(assetId);
                    if (assetScope == AssetScope::kStage)
                    {
                        Dia::Core::StringCRC stageId = runtime.GetAssetStageId(assetId);
                        if (stageId.Value() != 0)
                            root["stageId"] = stageId.AsChar();
                    }

                    Json::FastWriter writer;
                    DIA_LOG_INFO("AssetRuntimeDebugCommands", "%s", writer.write(root).c_str());
                    return path ? 0 : 1;
                };
                Dia::API::RegisterCommand(cmd);
            }

            // ----------------------------------------------------------------
            // asset_runtime.get-stage-deps  --stageId=<id>
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-get-stage-deps");
                cmd.description = "List all asset IDs belonging to a stage";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-get-stage-deps --stageId=stage.gameplay";
                cmd.callback    = [&runtime](const Dia::API::CommandArgs& args) -> int
                {
                    auto it = args.namedArgs.find(Dia::Core::StringCRC("stageId").Value());
                    if (it == args.namedArgs.end() || !it->second)
                    {
                        DIA_LOG_WARNING("AssetRuntimeDebugCommands",
                            "asset-runtime-get-stage-deps: missing --stageId parameter");
                        return 1;
                    }

                    Dia::Core::StringCRC stageId(it->second);
                    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> results;
                    unsigned int total = runtime.GetStageDependencies(stageId, results);

                    Json::Value root;
                    root["stageId"] = it->second;
                    root["total"]   = total;
                    Json::Value& assets = root["assets"];
                    assets = Json::Value(Json::arrayValue);
                    for (unsigned int i = 0; i < results.Size(); ++i)
                        assets.append(results[i].AsChar());

                    Json::FastWriter writer;
                    DIA_LOG_INFO("AssetRuntimeDebugCommands", "%s", writer.write(root).c_str());
                    return (total > 0) ? 0 : 1;
                };
                Dia::API::RegisterCommand(cmd);
            }

            // ----------------------------------------------------------------
            // asset_runtime.get-all-states
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-get-all-states");
                cmd.description = "Full runtime snapshot: state, scope, ref count and path for every asset";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-get-all-states";
                cmd.callback    = [&runtime](const Dia::API::CommandArgs&) -> int
                {
                    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> all;
                    unsigned int total = runtime.GetAllAssets(all);

                    Json::Value root;
                    Json::Value& assets = root["assets"];
                    assets = Json::Value(Json::arrayValue);

                    for (unsigned int i = 0; i < all.Size(); ++i)
                    {
                        const Dia::Core::StringCRC& id = all[i];
                        AssetState state = runtime.GetAssetState(id);
                        AssetScope scope = runtime.GetAssetScope(id);
                        Json::Value entry;
                        entry["assetId"]    = id.AsChar();
                        entry["state"]      = StateToString(state);
                        entry["scope"]      = ScopeToString(scope);
                        entry["refCount"]   = runtime.GetAssetRefCount(id);
                        const Dia::Core::Containers::String512* path = runtime.ResolveAssetPath(id);
                        entry["deployPath"] = path ? path->AsCStr() : "";

                        if (scope == AssetScope::kStage)
                        {
                            Dia::Core::StringCRC stageId = runtime.GetAssetStageId(id);
                            if (stageId.Value() != 0)
                                entry["stageId"] = stageId.AsChar();
                        }

                        assets.append(entry);
                    }

                    root["total"] = total;

                    Json::FastWriter writer;
                    DIA_LOG_INFO("AssetRuntimeDebugCommands", "%s", writer.write(root).c_str());
                    return 0;
                };
                Dia::API::RegisterCommand(cmd);
            }

            // ----------------------------------------------------------------
            // asset_runtime.subscribe-transitions
            // Registers an IAssetStateListener that logs transitions and
            // pushes events via the TransitionNotifyCallback (DiaDebugServer).
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-subscribe-transitions");
                cmd.description = "Subscribe to asset state transitions (logs + push via DiaDebugServer)";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-subscribe-transitions";
                cmd.callback    = [&runtime](const Dia::API::CommandArgs&) -> int
                {
                    if (!sTransitionLoggerRegistered)
                    {
                        runtime.RegisterListener(&sTransitionLogger);
                        sTransitionLoggerRegistered = true;
                        DIA_LOG_INFO("AssetRuntimeDebugCommands",
                            "subscribe_transitions: transition logging enabled");
                    }
                    else
                    {
                        DIA_LOG_INFO("AssetRuntimeDebugCommands",
                            "subscribe_transitions: already subscribed");
                    }
                    return 0;
                };
                Dia::API::RegisterCommand(cmd);
            }
        }

    } // namespace AssetRuntime
} // namespace Dia
