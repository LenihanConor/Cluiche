#include "DiaDebugServer/CommandDispatcher.h"
#include "DiaDebugServer/DebugServerModule.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>

namespace Dia
{
	namespace DebugServer
	{
		CommandDispatcher::CommandDispatcher()
		{
		}

		Json::Value CommandDispatcher::ExecuteDiaAPICommand(const Dia::Core::StringCRC& commandName,
		                                                    const Json::Value& argsJson)
		{
			Json::Value response;

			const Dia::API::CommandInfo* cmdInfo = Dia::API::GetCommand(commandName);
			if (!cmdInfo)
			{
				response["success"] = false;
				response["message"] = "Unknown command";
				return response;
			}

			Dia::API::CommandArgs args;

			if (argsJson.isObject())
			{
				Json::Value::Members members = argsJson.getMemberNames();
				for (unsigned int i = 0; i < members.size(); ++i)
				{
					Dia::Core::StringCRC keyCrc(members[i].c_str());
					unsigned int keyHash = keyCrc.Value();

					if (argsJson[members[i]].isBool())
					{
						args.flags[keyHash] = argsJson[members[i]].asBool();
					}
					else if (argsJson[members[i]].isString())
					{
						args.namedArgs[keyHash] = argsJson[members[i]].asCString();
					}
				}
			}

			int result = cmdInfo->callback(args);
			response["success"] = (result == 0);
			response["message"] = (result == 0) ? "Command executed successfully" : "Command failed";
			response["result_code"] = result;
			return response;
		}

		Json::Value CommandDispatcher::ExecuteProtocolCommand(const Dia::Core::StringCRC& commandName,
		                                                      const Json::Value& payload,
		                                                      DebugServerModule* server)
		{
			Json::Value response;

			auto it = mProtocolHandlers.find(commandName);
			if (it != mProtocolHandlers.end())
			{
				it->second(payload, response);
				return response;
			}

			response["success"] = false;
			response["message"] = "Unknown protocol command";
			return response;
		}

		void CommandDispatcher::RegisterProtocolCommand(const Dia::Core::StringCRC& commandName,
		                                                ProtocolCommandHandler handler)
		{
			mProtocolHandlers[commandName] = handler;
		}

		void CommandDispatcher::UnregisterProtocolCommand(const Dia::Core::StringCRC& commandName)
		{
			mProtocolHandlers.erase(commandName);
		}

		bool CommandDispatcher::IsProtocolCommand(const Dia::Core::StringCRC& commandName) const
		{
			return mProtocolHandlers.find(commandName) != mProtocolHandlers.end();
		}
	}
}
