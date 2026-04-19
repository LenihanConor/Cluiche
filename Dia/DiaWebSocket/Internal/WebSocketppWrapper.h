#ifndef DIA_WEBSOCKET_INTERNAL_WEBSOCKETPP_WRAPPER_H
#define DIA_WEBSOCKET_INTERNAL_WEBSOCKETPP_WRAPPER_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from size_t
#pragma warning(disable: 4244) // conversion, possible loss of data
#pragma warning(disable: 4127) // conditional expression is constant
#endif

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#ifndef _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_CPP11_STL_
#endif
#ifndef _WEBSOCKETPP_CPP11_THREAD_
#define _WEBSOCKETPP_CPP11_THREAD_
#endif

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Windows headers pulled in by ASIO define macros that clash with DiaCore
#ifdef Yield
#undef Yield
#endif

#include "DiaWebSocket/Message.h"
#include "DiaWebSocket/Error.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace WebSocket
	{
		namespace Internal
		{
			using WsppServer = websocketpp::server<websocketpp::config::asio>;
			using WsppClient = websocketpp::client<websocketpp::config::asio_client>;
			using ConnectionHdl = websocketpp::connection_hdl;

			struct QueuedEvent
			{
				enum class Type
				{
					kMessage,
					kConnected,
					kDisconnected,
					kError
				};

				Type eventType;
				int connectionId;
				Message message;
				Error error;
				Dia::Core::Containers::DynamicArrayC<char, 1024> dataBuffer;
			};

			struct OutgoingMessage
			{
				int connectionId; // -1 = broadcast
				MessageType type;
				Dia::Core::Containers::DynamicArrayC<char, 1024> dataBuffer;
				size_t dataLength;
			};

			static const size_t kDefaultMaxMessageSize = 1024 * 1024; // 1MB
			static const int kDefaultMaxConnections = 16;
			static const int kDefaultQueueCapacity = 64;
		}
	}
}

#endif // DIA_WEBSOCKET_INTERNAL_WEBSOCKETPP_WRAPPER_H
