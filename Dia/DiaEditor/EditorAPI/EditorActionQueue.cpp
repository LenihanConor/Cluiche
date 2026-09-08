#include "DiaEditor/EditorAPI/EditorActionQueue.h"
#include <DiaObservation/Log/DiaLog.h>
#include <chrono>

namespace Dia
{
	namespace Editor
	{
		const float EditorActionQueue::kTimeoutSeconds = 5.0f;

		Json::Value EditorActionQueue::DispatchAndWait(Dia::Core::StringCRC name, const Json::Value& params)
		{
			// If called from the main thread, execute inline — pushing to the queue
			// would deadlock because DoUpdate() (the queue drainer) also runs on the
			// main thread and can't run while we're blocking here.
			if (std::this_thread::get_id() == mMainThreadId && mRegistry != nullptr)
			{
				try
				{
					return mRegistry->ExecuteAction(name, params);
				}
				catch (const std::exception& e)
				{
					Json::Value err;
					err["success"] = false;
					err["reason"]  = e.what();
					return err;
				}
				catch (...)
				{
					Json::Value err;
					err["success"] = false;
					err["reason"]  = "unknown_exception";
					return err;
				}
			}

			PendingAction* item = new PendingAction();
			item->name   = name;
			item->params = params;

			std::future<Json::Value> future = item->promise.get_future();

			{
				std::lock_guard<std::mutex> lock(mMutex);
				mQueue.push(item);
			}

			const auto status = future.wait_for(std::chrono::duration<float>(kTimeoutSeconds));
			if (status == std::future_status::timeout)
			{
				DIA_LOG_WARNING("EditorActionQueue", "Action dispatch timed out waiting for main thread");
				Json::Value err;
				err["success"] = false;
				err["reason"]  = "timeout";
				return err;
			}

			return future.get();
		}

		void EditorActionQueue::DoUpdate(EditorActionRegistry* registry, float /*deltaTime*/)
		{
			// Capture main thread identity and registry on first call so DispatchAndWait
			// can execute inline when called from the main thread.
			if (mMainThreadId == std::thread::id{})
			{
				mMainThreadId = std::this_thread::get_id();
				mRegistry     = registry;
			}

			std::queue<PendingAction*> local;
			{
				std::lock_guard<std::mutex> lock(mMutex);
				std::swap(local, mQueue);
			}

			while (!local.empty())
			{
				PendingAction* item = local.front();
				local.pop();

				Json::Value result;
				try
				{
					result = registry->ExecuteAction(item->name, item->params);
				}
				catch (const std::exception& e)
				{
					DIA_LOG_ERROR("EditorActionQueue", "Handler threw exception");
					result["success"] = false;
					result["reason"]  = e.what();
				}
				catch (...)
				{
					DIA_LOG_ERROR("EditorActionQueue", "Handler threw unknown exception");
					result["success"] = false;
					result["reason"]  = "unknown_exception";
				}

				item->promise.set_value(result);
				delete item;
			}
		}

		unsigned int EditorActionQueue::GetDepth() const
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return static_cast<unsigned int>(mQueue.size());
		}

	} // namespace Editor
} // namespace Dia
