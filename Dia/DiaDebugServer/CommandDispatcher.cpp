#include "DiaDebugServer/CommandDispatcher.h"

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
						args.SetFlag(keyHash, argsJson[members[i]].asBool());
					}
					else if (argsJson[members[i]].isString())
					{
						args.SetNamedArg(keyHash, argsJson[members[i]].asCString());
					}
				}
			}

			int result = cmdInfo->callback(args);
			response["success"] = (result == 0);
			response["message"] = (result == 0) ? "Command executed successfully" : "Command failed";
			response["result_code"] = result;
			return response;
		}
	}
}
