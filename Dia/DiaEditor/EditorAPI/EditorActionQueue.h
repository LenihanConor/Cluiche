#pragma once

#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <mutex>
#include <queue>
#include <future>
#include <functional>

namespace Dia
{
	namespace Editor
	{
		// Thread-safe queue for dispatching editor actions to the main thread.
		// Callers push a PendingAction and block on the returned future.
		// DoUpdate() (called from main thread each frame) drains the queue.
		class EditorActionQueue
		{
		public:
			static const float kTimeoutSeconds;  // 5.0f

			// Posted by any thread; resolved on the main thread by DoUpdate().
			struct PendingAction
			{
				Dia::Core::StringCRC          name;
				Json::Value                   params;
				std::promise<Json::Value>     promise;
			};

			// Push an action and block until DoUpdate() resolves it.
			// Returns {"success":false,"reason":"timeout"} if DoUpdate() does not
			// resolve within kTimeoutSeconds.
			Json::Value DispatchAndWait(Dia::Core::StringCRC name, const Json::Value& params);

			// Called from the main thread each frame.
			// Drains all pending items, invokes registry->ExecuteAction(), resolves promises.
			// deltaTime is seconds since last update (used to track drain timing).
			void DoUpdate(EditorActionRegistry* registry, float deltaTime);

			// Gauge value — number of items currently pending (approximate, for metrics).
			unsigned int GetDepth() const;

		private:
			mutable std::mutex                   mMutex;
			std::queue<PendingAction*>            mQueue;
		};

	} // namespace Editor
} // namespace Dia
