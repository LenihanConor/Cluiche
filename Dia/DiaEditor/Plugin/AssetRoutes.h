#pragma once

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
	namespace Editor
	{
		// Helpers for plugins (typically game-scoped ones) that route an asset type to an editor plugin while
		// they are loaded: call RegisterAssetRoute in OnLoad and UnregisterAssetRoute in OnUnload.
		//
		// They call the asset_catalogue.register_type_editor / unregister_type_editor bridge handlers. The handlers
		// are only present while DiaAssetCatalogueEditor is loaded; when absent (or bridge is null) the call is a
		// no-op that returns false and logs at Info. Routes are not replayed if the catalogue plugin loads later.
		// The catalogue never overrides an existing route and only removes a route mapped to the same editor, so
		// built-in routes cannot be removed through here.
		namespace AssetRoutes
		{
			inline bool CallRoute(WebUIBridge* bridge, const char* handler, const char* responseKey,
			                      const char* assetType, const char* editorPluginType)
			{
				if (bridge == nullptr || assetType == nullptr || editorPluginType == nullptr)
					return false;

				Json::Value req(Json::objectValue);
				req["assetType"]        = assetType;
				req["editorPluginType"] = editorPluginType;
				const Json::Value res = bridge->InvokeRequestHandler(Dia::Core::StringCRC(handler), req);
				if (res.isNull())
				{
					DIA_LOG_INFO("Editor", "AssetRoutes: %s skipped for '%s' (asset catalogue plugin not loaded)", handler, assetType);
					return false;
				}
				return res.get("success", false).asBool() && res.get(responseKey, false).asBool();
			}

			// Route assetType to editorPluginType. Returns true if the route was added.
			inline bool RegisterAssetRoute(WebUIBridge* bridge, const char* assetType, const char* editorPluginType)
			{
				return CallRoute(bridge, "asset_catalogue.register_type_editor", "registered", assetType, editorPluginType);
			}

			// Remove a route previously added with RegisterAssetRoute. Returns true if a route was removed.
			inline bool UnregisterAssetRoute(WebUIBridge* bridge, const char* assetType, const char* editorPluginType)
			{
				return CallRoute(bridge, "asset_catalogue.unregister_type_editor", "removed", assetType, editorPluginType);
			}
		}
	}
}
