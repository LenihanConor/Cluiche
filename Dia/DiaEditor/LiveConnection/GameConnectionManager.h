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
			using RawMessageCallback = std::function<void(const Json::Value&)>;

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

			// Raw message channel: delivers every inbound JSON message the client
			// receives, regardless of envelope shape. Used by consumers that speak
			// a non-topic protocol (e.g. DiaDebugProtocol's {type, ...} frames).
			void SetRawMessageCallback(RawMessageCallback callback);
			void SendRaw(const Json::Value& message);
			const char* GetLastError() const { return mLastError; }

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
			RawMessageCallback mRawMessageCallback;

			char mHost[128];
			int mPort;

			static const unsigned int kMaxErrorLength = 256;
			char mLastError[kMaxErrorLength];

			static const float kInitialReconnectDelay;
			static const float kMaxReconnectDelay;
		};
	}
}
