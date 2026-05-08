#include "DiaAssetRuntime/AssetRuntimeDebugCommands.h"
#include "DiaAssetRuntime/AssetRuntime.h"
#include "DiaAssetRuntime/AssetState.h"
#include "DiaAssetRuntime/AssetScope.h"
#include "DiaAssetRuntime/RuntimeAssetEntry.h"

#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaLogger/DiaLog.h>

namespace
{
    const char* StateToString(Dia::AssetRuntime::AssetState state)
    {
        switch (state)
        {
            case Dia::AssetRuntime::AssetState::Null:     return "Null";
            case Dia::AssetRuntime::AssetState::Staged:   return "Staged";
            case Dia::AssetRuntime::AssetState::Loading:  return "Loading";
            case Dia::AssetRuntime::AssetState::Loaded:   return "Loaded";
            case Dia::AssetRuntime::AssetState::Failed:   return "Failed";
            case Dia::AssetRuntime::AssetState::Unloaded: return "Unloaded";
            default:                                       return "Unknown";
        }
    }

    const char* ScopeToString(Dia::AssetRuntime::AssetScope scope)
    {
        return (scope == Dia::AssetRuntime::AssetScope::kGlobal) ? "Global" : "Stage";
    }

} // anonymous namespace

namespace Dia
{
    namespace AssetRuntime
    {
        void RegisterAssetRuntimeCommands(AssetRuntime& runtime, ITransitionNotifier* notifier)
        {
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
                    const char* assetIdStr = args.GetNamedArg(Dia::Core::StringCRC("assetId").Value());
                    if (!assetIdStr)
                    {
                        DIA_LOG_WARNING("AssetRuntimeDebugCommands",
                            "asset-runtime-get-state: missing --assetId parameter");
                        return 1;
                    }

                    Dia::Core::StringCRC assetId(assetIdStr);
                    AssetState state = runtime.GetAssetState(assetId);
                    unsigned int refCount = runtime.GetAssetRefCount(assetId);
                    const Dia::Core::Containers::String512* path = runtime.ResolveAssetPath(assetId);

                    Json::Value root;
                    root["assetId"]  = assetIdStr;
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
                    const char* stageIdStr = args.GetNamedArg(Dia::Core::StringCRC("stageId").Value());
                    if (!stageIdStr)
                    {
                        DIA_LOG_WARNING("AssetRuntimeDebugCommands",
                            "asset-runtime-get-stage-deps: missing --stageId parameter");
                        return 1;
                    }

                    Dia::Core::StringCRC stageId(stageIdStr);
                    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> results;
                    unsigned int total = runtime.GetStageDependencies(stageId, results);

                    Json::Value root;
                    root["stageId"] = stageIdStr;
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
            // Note: With the handler-based architecture, transition logging
            // is handled via DiaLogger. This command is a placeholder for
            // future WebSocket push via ITransitionNotifier.
            // ----------------------------------------------------------------
            {
                Dia::API::CommandInfo cmd;
                cmd.name        = Dia::Core::StringCRC("asset-runtime-subscribe-transitions");
                cmd.description = "Subscribe to asset state transitions (logs + push via DiaDebugServer)";
                cmd.category    = Dia::Core::StringCRC("asset-runtime");
                cmd.owner       = "DiaAssetRuntime";
                cmd.version     = "1.0.0";
                cmd.example     = "asset-runtime-subscribe-transitions";
                cmd.callback    = [notifier](const Dia::API::CommandArgs&) -> int
                {
                    DIA_LOG_INFO("AssetRuntimeDebugCommands",
                        "subscribe_transitions: transition logging enabled (via DiaLogger)");
                    (void)notifier;
                    return 0;
                };
                Dia::API::RegisterCommand(cmd);
            }
        }

    } // namespace AssetRuntime
} // namespace Dia
