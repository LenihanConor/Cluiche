#include "Plugins/DiaChatPlugin/ChatPanelBridge.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Trace/DiaTrace.h>

const Dia::Core::StringCRC CluicheEditor::ChatPanelBridgeHealth::kName("DiaChatPlugin");

namespace CluicheEditor
{
	void ChatPanelBridgeHealth::OnBackendAvailability(bool available)
	{
		if (available)
			SetOK();
		else
			SetFailing(Dia::Core::StringCRC("backend_unavailable"));
	}

	ChatPanelBridge::ChatPanelBridge(Dia::Editor::WebUIBridge* bridge)
		: mBridge(bridge)
	{
		auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricMessagesSent   = reg.RegisterCounter(Dia::Core::StringCRC("dia.chat.messages_sent"));
		mMetricToolCalls      = reg.RegisterCounter(Dia::Core::StringCRC("dia.chat.tool_calls"));
		mMetricTokensStreamed  = reg.RegisterCounter(Dia::Core::StringCRC("dia.chat.tokens_streamed"));

		Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
	}

	void ChatPanelBridge::Initialize(Dia::Editor::WebUIBridge* bridge)
	{
		mBridge = bridge;
	}

	ChatPanelBridge::~ChatPanelBridge()
	{
		Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
	}

	void ChatPanelBridge::OnTokenChunk(const char* text, bool done)
	{
		Json::Value payload;
		payload["text"] = text ? text : "";
		payload["done"] = done;

		if (text && text[0] != '\0' && mMetricTokensStreamed)
			mMetricTokensStreamed->Inc();
		if (done && mMetricMessagesSent)
			mMetricMessagesSent->Inc();

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kToken, std::move(payload) });
	}

	void ChatPanelBridge::OnToolStart(const char* callId, const char* fn, const Json::Value& params)
	{
		Json::Value payload;
		payload["call_id"] = callId ? callId : "";
		payload["fn"]      = fn ? fn : "";
		payload["params"]  = params;

		if (mMetricToolCalls) mMetricToolCalls->Inc();

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kToolStart, std::move(payload) });
	}

	void ChatPanelBridge::OnToolResult(const char* callId, const Json::Value& result, int durationMs)
	{
		Json::Value payload;
		payload["call_id"]     = callId ? callId : "";
		payload["result"]      = result;
		payload["duration_ms"] = durationMs;

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kToolResult, std::move(payload) });
	}

	void ChatPanelBridge::OnToolError(const char* callId, const char* error)
	{
		Json::Value payload;
		payload["call_id"] = callId ? callId : "";
		payload["error"]   = error ? error : "";

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kToolError, std::move(payload) });
	}

	void ChatPanelBridge::OnConfirmRequired(const char* callId, const char* fn, const Json::Value& params, const char* description)
	{
		Json::Value payload;
		payload["call_id"]     = callId ? callId : "";
		payload["fn"]          = fn ? fn : "";
		payload["params"]      = params;
		payload["description"] = description ? description : "";

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kConfirmRequired, std::move(payload) });
	}

	void ChatPanelBridge::OnChatError(const char* message)
	{
		Json::Value payload;
		payload["message"] = message ? message : "";

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kChatError, std::move(payload) });
	}

	void ChatPanelBridge::OnBackendStatus(const char* backend, const char* model, bool available)
	{
		Json::Value payload;
		payload["backend"]   = backend ? backend : "";
		payload["model"]     = model ? model : "";
		payload["available"] = available;

		mHealth.OnBackendAvailability(available);

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kBackendStatus, std::move(payload) });
	}

	void ChatPanelBridge::OnContextWarning(int usedTokens, int budgetTokens, int pct)
	{
		Json::Value payload;
		payload["used_tokens"]   = usedTokens;
		payload["budget_tokens"] = budgetTokens;
		payload["pct"]           = pct;

		std::lock_guard<std::mutex> lock(mQueueMutex);
		mEventQueue.push({ EventType::kContextWarning, std::move(payload) });
	}

	void ChatPanelBridge::DoUpdate(float /*deltaTime*/)
	{
		DIA_TRACE_ZONE("chat.bridge_drain", Dia::Observation::Trace::Category::kDiaApplicationFlow);

		if (mBridge == nullptr)
			return;

		std::queue<ChatEvent> local;
		{
			std::lock_guard<std::mutex> lock(mQueueMutex);
			std::swap(local, mEventQueue);
		}

		if (!local.empty())
			DIA_LOG_DEBUG("Chat", "ChatPanelBridge::DoUpdate: draining %u events", static_cast<unsigned>(local.size()));

		while (!local.empty())
		{
			ChatEvent& ev = local.front();

			switch (ev.type)
			{
			case EventType::kToken:
				mBridge->NotifyUIDataChanged("chat.token", ev.payload);
				break;
			case EventType::kToolStart:
				mBridge->NotifyUIDataChanged("chat.tool_start", ev.payload);
				break;
			case EventType::kToolResult:
				mBridge->NotifyUIDataChanged("chat.tool_result", ev.payload);
				break;
			case EventType::kToolError:
				mBridge->NotifyUIDataChanged("chat.tool_error", ev.payload);
				break;
			case EventType::kConfirmRequired:
				mBridge->NotifyUIDataChanged("chat.confirm_required", ev.payload);
				break;
			case EventType::kChatError:
				mBridge->NotifyUIDataChanged("chat.error", ev.payload);
				break;
			case EventType::kBackendStatus:
				mBridge->NotifyUIDataChanged("chat.backend_status", ev.payload);
				break;
			case EventType::kContextWarning:
				mBridge->NotifyUIDataChanged("chat.context_warning", ev.payload);
				break;
			default:
				DIA_LOG_INFO("Chat", "ChatPanelBridge::DoUpdate: unknown event type");
				break;
			}

			local.pop();
		}
	}
}
