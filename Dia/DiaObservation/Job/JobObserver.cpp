#include <DiaObservation/Job/JobObserver.h>

#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Profile/ScopedZone.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Trace/Tracer.h>

#include <cstdio>
#include <functional>
#include <new>
#include <thread>

namespace Dia
{
	namespace Observation
	{
		namespace Job
		{
			namespace
			{
				thread_local JobInbox* tInbox = nullptr;

				// Stack of open `job.run` scopes on this thread (Begin/End are separate calls;
				// inline/nested execution pushes more than one).
				constexpr unsigned kMaxNestedJobScopes = 8;
				struct ScopeSlot { alignas(Profile::ScopedZone) unsigned char storage[sizeof(Profile::ScopedZone)]; };
				thread_local ScopeSlot tScopes[kMaxNestedJobScopes];
				thread_local void*     tJobInboxes[kMaxNestedJobScopes] = {};  // inbox of each running job (nested submits inherit)
				thread_local unsigned  tScopeDepth = 0;   // logical depth (may exceed capacity)
			}

			void JobObserver::InstallThreadInbox(JobInbox* inbox) { tInbox = inbox; }
			void JobObserver::ClearThreadInbox() { tInbox = nullptr; }

			void JobObserver::OnWorkerStart(unsigned workerIndex)
			{
				Log::Logger::Instance().RegisterThreadBuffer();
				Trace::Tracer::Instance().RegisterThreadSpanBuffer();
				Profile::Profiler::Instance().RegisterThreadScopeBuffer();
				char name[Profile::Profiler::kThreadNameLen];
				snprintf(name, sizeof(name), "Worker %u", workerIndex);
				Profile::Profiler::RegisterThreadName(Profile::Profiler::CurrentThreadId(), name);
			}

			void JobObserver::OnWorkerStop(unsigned /*workerIndex*/)
			{
				Profile::Profiler::Instance().UnregisterThreadScopeBuffer();
				Trace::Tracer::Instance().UnregisterThreadSpanBuffer();
				Log::Logger::Instance().UnregisterThreadBuffer();
			}

			Dia::Threading::JobContext JobObserver::CaptureContext()
			{
				Dia::Threading::JobContext ctx;
				ctx.attribution.owner = Profile::Profiler::CurrentSubject();
				ctx.traceParent = 0;
				ctx.submitThread = static_cast<uint32_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
				ctx.inbox = tInbox;
				if (!ctx.inbox && tScopeDepth > 0 && tScopeDepth <= kMaxNestedJobScopes)
					ctx.inbox = tJobInboxes[tScopeDepth - 1];   // nested submit from a worker
				return ctx;
			}

			void JobObserver::OnJobBegin(const Dia::Threading::JobContext& ctx)
			{
				if (tScopeDepth < kMaxNestedJobScopes)
				{
					tJobInboxes[tScopeDepth] = ctx.inbox;
					new (tScopes[tScopeDepth].storage) Profile::ScopedZone(
						Dia::Core::StringCRC("job.run"), Profile::Category::kAll, ctx.attribution.owner);
				}
				++tScopeDepth;
			}

			void JobObserver::OnJobEnd(const Dia::Threading::JobContext& ctx,
			                           const Dia::Threading::JobTimingRecord& rec)
			{
				if (tScopeDepth > 0)
				{
					--tScopeDepth;
					if (tScopeDepth < kMaxNestedJobScopes)
						reinterpret_cast<Profile::ScopedZone*>(tScopes[tScopeDepth].storage)->~ScopedZone();
				}

				if (ctx.inbox)
					static_cast<JobInbox*>(ctx.inbox)->TryPush(rec);
				else
					mUnowned.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}
}
