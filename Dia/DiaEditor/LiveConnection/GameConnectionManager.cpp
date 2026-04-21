#include "DiaEditor/LiveConnection/GameConnectionManager.h"

#include <DiaWebSocket/Client.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>

#include <string.h>
#include <sstream>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC GameConnectionManager::kUniqueId("GameConnectionManager");
		const float GameConnectionManager::kInitialReconnectDelay = 1.0f;
		const float GameConnectionManager::kMaxReconnectDelay = 10.0f;

		GameConnectionManager::GameConnectionManager()
			: mClient(nullptr)
			, mPort(0)
		{
			mHost[0] = '\0';
			mLastError[0] = '\0';
		}

		GameConnectionManager::~GameConnectionManager()
		{
			Shutdown();
		}

		void GameConnectionManager::Initialize()
		{
			Dia::Core::Log::OutputVaradicLine("GameConnectionManager: Initialize");
			mClient = new Dia::WebSocket::Client();
			mClient->SetReconnectOnDisconnect(false);

			mClient->SetMessageCallback([this](const Dia::WebSocket::Message& msg) {
				if (msg.type == Dia::WebSocket::MessageType::kText)
				{
					Dia::Core::Log::OutputVaradicLine("GameConnectionManager: Received text message (%u bytes)", static_cast<unsigned>(msg.length));
					HandleMessage(std::string(static_cast<const char*>(msg.data), msg.length));
				}
			});

			mClient->SetConnectionCallback([this](bool connected) {
				HandleConnection(connected);
			});
		}

		void GameConnectionManager::Shutdown()
		{
			Dia::Core::Log::OutputVaradicLine("GameConnectionManager: Shutdown");
			if (mClient != nullptr)
			{
				mClient->Disconnect();
				delete mClient;
				mClient = nullptr;
			}
		}

		void GameConnectionManager::Update(float /*deltaTime*/)
		{
			if (mClient != nullptr)
				mClient->Update();
		}

		void GameConnectionManager::Connect(const char* host, int port)
		{
			DIA_ASSERT(mClient != nullptr, "GameConnectionManager: must call Initialize() before Connect()");
			strncpy_s(mHost, sizeof(mHost), host, _TRUNCATE);
			mPort = port;

			std::ostringstream url;
			url << "ws://" << host << ":" << port;
			Dia::Core::Log::OutputVaradicLine("GameConnectionManager: Connect to %s", url.str().c_str());
			mClient->Connect(url.str().c_str());
		}

		void GameConnectionManager::Disconnect()
		{
			Dia::Core::Log::OutputVaradicLine("GameConnectionManager: Disconnect requested");
			if (mClient != nullptr)
				mClient->Disconnect();
		}

		bool GameConnectionManager::IsConnected() const
		{
			return mClient != nullptr && mClient->IsConnected();
		}

		void GameConnectionManager::Subscribe(const Dia::Core::StringCRC& topic, DataCallback callback)
		{
			DIA_ASSERT(!mSubscriptions.IsFull(), "GameConnectionManager: max subscription capacity reached");
			Subscription sub;
			sub.topic = topic;
			sub.callback = callback;
			mSubscriptions.Add(sub);
		}

		void GameConnectionManager::Publish(const Dia::Core::StringCRC& topic, const Json::Value& data)
		{
			if (!IsConnected())
				return;

			Json::Value envelope;
			envelope["topic"] = topic.AsChar();
			envelope["data"] = data;

			Json::StreamWriterBuilder writer;
			std::string text = Json::writeString(writer, envelope);
			mClient->SendText(text.c_str());
		}

		void GameConnectionManager::SetConnectionCallback(ConnectionCallback callback)
		{
			mConnectionCallback = callback;
		}

		void GameConnectionManager::SetRawMessageCallback(RawMessageCallback callback)
		{
			mRawMessageCallback = callback;
		}

		void GameConnectionManager::SendRaw(const Json::Value& message)
		{
			if (!IsConnected())
			{
				Dia::Core::Log::OutputVaradicLine("GameConnectionManager: SendRaw skipped, not connected");
				return;
			}
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string text = Json::writeString(writer, message);
			Dia::Core::Log::OutputVaradicLine("GameConnectionManager: SendRaw %u bytes", static_cast<unsigned>(text.size()));
			mClient->SendText(text.c_str());
		}

		void GameConnectionManager::HandleMessage(const std::string& text)
		{
			Json::Value envelope;
			Json::CharReaderBuilder builder;
			std::string errors;
			std::istringstream stream(text);
			if (!Json::parseFromStream(builder, stream, &envelope, &errors))
				return;

			// Raw channel sees every message regardless of envelope shape.
			if (mRawMessageCallback)
				mRawMessageCallback(envelope);

			// Topic-based routing is only possible for messages that carry a topic.
			if (envelope.isMember("topic") && envelope["topic"].isString())
			{
				std::string topicStr = envelope["topic"].asString();
				Dia::Core::StringCRC topic(topicStr.c_str());
				const Json::Value& data = envelope["data"];

				for (unsigned int i = 0; i < mSubscriptions.Size(); ++i)
				{
					if (mSubscriptions[i].topic == topic)
						mSubscriptions[i].callback(data);
				}
			}
		}

		void GameConnectionManager::HandleConnection(bool connected)
		{
			Dia::Core::Log::OutputVaradicLine("GameConnectionManager: HandleConnection connected=%d", connected ? 1 : 0);
			if (connected)
				mLastError[0] = '\0';
			if (mConnectionCallback)
				mConnectionCallback(connected);
		}
	}
}
