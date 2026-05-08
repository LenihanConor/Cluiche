#include "DiaDebugServer/QueryRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace DebugServer
	{
		void QueryRegistry::Register(const Dia::Core::StringCRC& name, QueryHandler handler)
		{
			DIA_ASSERT(!Has(name), "QueryRegistry: handler already registered for '%s'", name.AsChar());
			DIA_ASSERT(!mHandlers.IsFull(), "QueryRegistry: max handler capacity reached");

			Entry entry;
			entry.name = name;
			entry.handler = handler;
			mHandlers.Add(entry);
		}

		void QueryRegistry::Unregister(const Dia::Core::StringCRC& name)
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
				{
					mHandlers.RemoveAt(i);
					return;
				}
			}
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
