#ifndef DIA_DEBUG_SERVER_COMMAND_DISPATCHER_H
#define DIA_DEBUG_SERVER_COMMAND_DISPATCHER_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <functional>
#include <unordered_map>

namespace dia { namespace debug { class CommandResponse; } }

namespace Dia
{
	namespace DebugServer
	{
		class DebugServerModule;

		using ProtocolCommandHandler = std::function<void(const Json::Value& payload, dia::debug::CommandResponse* responseOut)>;

		class CommandDispatcher
		{
		public:
			CommandDispatcher();

			Json::Value ExecuteDiaAPICommand(const Dia::Core::StringCRC& commandName,
			                                 const Json::Value& argsJson);

			void ExecuteProtocolCommand(const Dia::Core::StringCRC& commandName,
			                            const Json::Value& payload,
			                            DebugServerModule* server,
			                            dia::debug::CommandResponse* responseOut);

			void RegisterProtocolCommand(const Dia::Core::StringCRC& commandName,
			                             ProtocolCommandHandler handler);

			void UnregisterProtocolCommand(const Dia::Core::StringCRC& commandName);

			bool IsProtocolCommand(const Dia::Core::StringCRC& commandName) const;

		private:
			std::unordered_map<Dia::Core::StringCRC, ProtocolCommandHandler> mProtocolHandlers;
		};
	}
}

#endif // DIA_DEBUG_SERVER_COMMAND_DISPATCHER_H
