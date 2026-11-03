#include "DiaDebugServer/QueryRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace Dia
{
	namespace DebugServer
	{
		QueryRegistry::ThreadId QueryRegistry::CurrentThreadId()
		{
#if defined(_MSC_VER)
			return static_cast<ThreadId>(::GetCurrentThreadId());
#else
			return static_cast<ThreadId>(reinterpret_cast<uintptr_t>(pthread_self()) & 0xFFFFFFFFu);
#endif
		}

		bool QueryRegistry::RejectWrongThread(const char* op, const Dia::Core::StringCRC& name) const
		{
			if (!mHasOwner || CurrentThreadId() == mOwnerThread)
				return false;

			DIA_LOG_ERROR("DebugServer",
				"QueryRegistry: %s('%s') refused from thread %u (owner thread %u); register SimPU queries through SimQueryHost",
				op, name.AsChar(), static_cast<unsigned>(CurrentThreadId()), static_cast<unsigned>(mOwnerThread));
			if (auto* c = Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(
					Dia::Core::StringCRC("dia.debugserver.query.wrong_thread_register")))
				c->Inc();
			return true;
		}

		bool QueryRegistry::Register(const Dia::Core::StringCRC& name, QueryHandler handler)
		{
			if (RejectWrongThread("Register", name))
				return false;
			if (mVeto != nullptr && mVeto(mVetoContext, name))
			{
				DIA_LOG_ERROR("DebugServer", "QueryRegistry: '%s' is already sim-hosted, registration refused", name.AsChar());
				return false;
			}
			DIA_ASSERT(!Has(name), "QueryRegistry: handler already registered for '%s'", name.AsChar());
			DIA_ASSERT(!mHandlers.IsFull(), "QueryRegistry: max handler capacity reached");

			Entry entry;
			entry.name = name;
			entry.handler = handler;
			mHandlers.Add(entry);
			return true;
		}

		bool QueryRegistry::Unregister(const Dia::Core::StringCRC& name)
		{
			if (RejectWrongThread("Unregister", name))
				return false;
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
				{
					mHandlers.RemoveAt(i);
					return true;
				}
			}
			return false;
		}

		bool QueryRegistry::Has(const Dia::Core::StringCRC& name) const
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
					return true;
			}
			return false;
		}

		Json::Value QueryRegistry::Execute(const Dia::Core::StringCRC& name, const Json::Value& args) const
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
				{
					try
					{
						return mHandlers[i].handler(args);
					}
					catch (...)
					{
						DIA_LOG_ERROR("DebugServer", "QueryRegistry: handler for '%s' threw an exception", name.AsChar());
						Json::Value error;
						error["success"] = false;
						error["error"] = "handler failed";
						return error;
					}
				}
			}

			Json::Value notFound;
			notFound["success"] = false;
			notFound["error"] = "unknown query";
			return notFound;
		}
	}
}
