#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <functional>
#include <string>

namespace Dia
{
	namespace WebSocket
	{
		class Client;
	}

	namespace Editor
	{
		// Pure manager — no DiaApplication dependency.
		// Call Initialize() once on startup. Call Update() each frame.
		// Connection is on-demand: call Connect(host, port) when the game is ready.
		// The tool boots cleanly with no game present.
		class GameConnectionManager
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			using DataCallback = std::function<void(const Json::Value&)>;
			using ConnectionCallback = std::function<void(bool connected)>;

			GameConnectionManager();
			~GameConnectionManager();

			void Initialize();
			void Shutdown();
			void Update(float deltaTime);

			void Connect(const char* host, int port);
			void Disconnect();
			bool IsConnected() const;

			void Subscribe(const Dia::Core::StringCRC& topic, DataCallback callback);
			void Publish(const Dia::Core::StringCRC& topic, const Json::Value& data);

			void SetConnectionCallback(ConnectionCallback callback);

		private:
			void HandleMessage(const std::string& text);
			void HandleConnection(bool connected);

			struct Subscription
			{
				Dia::Core::StringCRC topic;
				DataCallback callback;
			};

			static const unsigned int kMaxSubscriptions = 32;
			Dia::Core::Containers::DynamicArrayC<Subscription, kMaxSubscriptions> mSubscriptions;

			Dia::WebSocket::Client* mClient;
			ConnectionCallback mConnectionCallback;

			char mHost[128];
			int mPort;

			static const float kInitialReconnectDelay;
			static const float kMaxReconnectDelay;
		};
	}
}
