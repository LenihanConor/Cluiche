#ifndef DIA_WEBSOCKET_CLIENT_H
#define DIA_WEBSOCKET_CLIENT_H

#include "DiaWebSocket/Message.h"
#include <functional>

namespace Dia
{
	namespace WebSocket
	{
		enum class ConnectionState
		{
			kDisconnected,
			kConnecting,
			kConnected,
			kError
		};

		class Client
		{
		public:
			Client();
			virtual ~Client();

			bool Connect(const char* url);
			void Disconnect();
			bool IsConnected() const;
			ConnectionState GetState() const;

			void Update();

			void Send(const void* data, size_t length, MessageType type = MessageType::kText);
			void SendText(const char* text);
			void SendBinary(const void* data, size_t length);

			void SetReconnectOnDisconnect(bool enable);
			void SetReconnectDelay(float seconds);
			void SetReconnectMaxAttempts(int maxAttempts);
			void SetConnectionTimeout(float seconds);
			void SetMaxMessageSize(size_t bytes);

			using MessageCallback = std::function<void(const Message& message)>;
			using ConnectionCallback = std::function<void(bool connected)>;
			using ErrorCallback = std::function<void(const Error& error)>;

			void SetMessageCallback(MessageCallback callback);
			void SetConnectionCallback(ConnectionCallback callback);
			void SetErrorCallback(ErrorCallback callback);

		private:
			Client(const Client&) = delete;
			Client& operator=(const Client&) = delete;

			struct Impl;
			Impl* mImpl;
		};
	}
}

#endif // DIA_WEBSOCKET_CLIENT_H
