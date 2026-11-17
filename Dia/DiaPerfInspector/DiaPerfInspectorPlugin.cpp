#include "DiaPerfInspector/DiaPerfInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		static const Dia::Core::StringCRC kReqQueryWindow("perf_inspector.query_window");
		static const Dia::Core::StringCRC kReqQueryFrame("perf_inspector.query_frame");

		static const Dia::Core::StringCRC kTopicPerfTiming("perf.timing");

		DiaPerfInspectorPlugin::DiaPerfInspectorPlugin()
			: LiveConnectionPluginBase({
				"Perf Inspector",
				"0.1",
				"Live frame and module timing for connected game instances",
				"dia://plugins/diaperfinspector/index.html",
				LayoutMode::kDockable,
				"",
				"P",
				false
			}, "perf_inspector")
		{
		}

		void DiaPerfInspectorPlugin::OnLivePluginLoad()
		{
			DIA_LOG_INFO("Inspector", "DiaPerfInspectorPlugin: loaded");

			RegisterHandler(kReqQueryWindow, [this](const Json::Value& d) { return HandleQueryWindow(d); });
			RegisterHandler(kReqQueryFrame,  [this](const Json::Value& d) { return HandleQueryFrame(d); });

			RegisterGameTopic(kTopicPerfTiming, [this](const Json::Value& d)
			{
				if (GetBridge())
					GetBridge()->NotifyUIDataChanged("perf_inspector.timing", d);
			});
		}

		void DiaPerfInspectorPlugin::OnLivePluginUnload()
		{
			DIA_LOG_INFO("Inspector", "DiaPerfInspectorPlugin: unloaded");
		}

		void DiaPerfInspectorPlugin::OnGameConnected()
		{
			DIA_LOG_INFO("Inspector", "Perf inspector: game connection established");
		}

		void DiaPerfInspectorPlugin::OnGameDisconnected()
		{
			DIA_LOG_INFO("Inspector", "Perf inspector: game connection lost");
		}

		Json::Value DiaPerfInspectorPlugin::HandleQueryWindow(const Json::Value& data)
		{
			Json::Value args;
			args["seconds"] = data.isMember("seconds") ? data["seconds"] : Json::Value(10.0);
			return RelayQuery("perf.window", args);
		}

		Json::Value DiaPerfInspectorPlugin::HandleQueryFrame(const Json::Value& data)
		{
			Json::Value args;
			args["unit"] = data.isMember("unit") ? data["unit"] : Json::Value("");
			args["frame"] = data.isMember("frame") ? data["frame"] : Json::Value(0);
			return RelayQuery("perf.frame", args);
		}

		// Sends a game command and pushes the reply to the UI as perf_inspector.query_result,
		// tagged with the queryId returned here so the UI can match it.
		Json::Value DiaPerfInspectorPlugin::RelayQuery(const char* command, const Json::Value& args)
		{
			Json::Value result;
			if (GetGameConnection() == nullptr || !GetGameConnection()->IsConnected())
			{
				result["ok"] = false;
				result["error"] = "not connected";
				return result;
			}

			const int queryId = mNextQueryId++;
			GetGameConnection()->SendCommandWithResponse(command, args,
				[this, queryId](bool success, const Json::Value& res)
				{
					Json::Value notification;
					notification["queryId"] = queryId;
					notification["success"] = success;
					notification["result"] = res;
					if (GetBridge())
						GetBridge()->NotifyUIDataChanged("perf_inspector.query_result", notification);
				});

			result["ok"] = true;
			result["queryId"] = queryId;
			return result;
		}
	}
}

using namespace Dia::Editor;
REGISTER_EDITOR_PLUGIN(DiaPerfInspectorPlugin, "DiaPerfInspector")
