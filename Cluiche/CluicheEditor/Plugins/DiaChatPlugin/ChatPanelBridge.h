#pragma once

#include <DiaCore/Json/external/json/json.h>

#include <mutex>
#include <queue>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
	}
}

namespace CluicheEditor
{
	class ChatPanelBridge
	{
	public:
		explicit ChatPanelBridge(Dia::Editor::WebUIBridge* bridge);

		void Initialize(Dia::Editor::WebUIBridge* bridge);

		// Called from DiaPython background thread — thread-safe push.
		void OnTokenChunk(const char* text, bool done);
		void OnToolStart(const char* callId, const char* fn, const Json::Value& params);
		void OnToolResult(const char* callId, const Json::Value& result, int durationMs);
		void OnToolError(const char* callId, const char* error);
		void OnConfirmRequired(const char* callId, const char* fn, const Json::Value& params, const char* description);
		void OnChatError(const char* message);
		void OnBackendStatus(const char* backend, const char* model, bool available);
		void OnContextWarning(int usedTokens, int budgetTokens, int pct);

		// Called from main thread (EditorPU update loop) — drains the token queue.
		void DoUpdate(float deltaTime);

	private:
		enum class EventType
		{
			kToken,
			kToolStart,
			kToolResult,
			kToolError,
			kConfirmRequired,
			kChatError,
			kBackendStatus,
			kContextWarning
		};

		struct ChatEvent
		{
			EventType   type;
			Json::Value payload;
		};

		Dia::Editor::WebUIBridge* mBridge;
		std::mutex                mQueueMutex;
		std::queue<ChatEvent>     mEventQueue;
	};
}
