#ifndef DIA_WEBSOCKET_SERVER_H
#define DIA_WEBSOCKET_SERVER_H

#include "DiaWebSocket/Message.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <functional>
#include <cstdint>

namespace Dia
{
	namespace WebSocket
	{
		class Server
		{
		public:
			Server(uint16_t port);
			virtual ~Server();

			bool Start();
			void Stop();
			bool IsRunning() const;
			void Update();

			void Broadcast(const void* data, size_t length, MessageType type = MessageType::kText);
			void Send(int connectionId, const void* data, size_t length, MessageType type = MessageType::kText);

			void BroadcastText(const char* text);
			void SendText(int connectionId, const char* text);
			void BroadcastBinary(const void* data, size_t length);
			void SendBinary(int connectionId, const void* data, size_t length);

			int GetConnectionCount() const;
			Dia::Core::Containers::DynamicArrayC<int, 16> GetActiveConnectionIds() const;
			void CloseConnection(int connectionId);

			void SetMaxConnections(int max);
			void SetMaxMessageSize(size_t bytes);

			using MessageCallback = std::function<void(int connectionId, const Message& message)>;
			using ConnectionCallback = std::function<void(int connectionId, bool connected)>;
			using ErrorCallback = std::function<void(const Error& error)>;

			void SetMessageCallback(MessageCallback callback);
			void SetConnectionCallback(ConnectionCallback callback);
			void SetErrorCallback(ErrorCallback callback);

		private:
			Server(const Server&) = delete;
			Server& operator=(const Server&) = delete;

			struct Impl;
			Impl* mImpl;
		};
	}
}

#endif // DIA_WEBSOCKET_SERVER_H
