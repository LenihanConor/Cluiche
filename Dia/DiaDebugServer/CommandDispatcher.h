#ifndef DIA_DEBUG_SERVER_COMMAND_DISPATCHER_H
#define DIA_DEBUG_SERVER_COMMAND_DISPATCHER_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace DebugServer
	{
		class CommandDispatcher
		{
		public:
			CommandDispatcher();

			Json::Value ExecuteDiaAPICommand(const Dia::Core::StringCRC& commandName,
			                                 const Json::Value& argsJson);
		};
	}
}

#endif // DIA_DEBUG_SERVER_COMMAND_DISPATCHER_H
