#include "DiaEditor/LiveConnection/GameConnectionManager.h"

#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>

#include <DiaWebSocket/Client.h>
#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

#include <string.h>
#include <stdio.h>
#include <string>

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
			, mAutoReconnect(false)
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
			DIA_LOG_INFO("Editor", "GameConnectionManager: Initialize");
			mClient = new Dia::WebSocket::Client();
			mClient->SetReconnectOnDisconnect(mAutoReconnect);

			mClient->SetMessageCallback([this](const Dia::WebSocket::Message& msg) {
				if (msg.type == Dia::WebSocket::MessageType::kText)
				{
					HandleMessage(static_cast<const char*>(msg.data), static_cast<unsigned int>(msg.length));
				}
			});

			mClient->SetConnectionCallback([this](bool connected) {
				HandleConnection(connected);
			});
		}

		void GameConnectionManager::Shutdown()
		{
			DIA_LOG_INFO("Editor", "GameConnectionManager: Shutdown");
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

		void GameConnectionManager::SetAutoReconnect(bool enable)
		{
			mAutoReconnect = enable;
			if (mClient != nullptr)
				mClient->SetReconnectOnDisconnect(enable);
		}

		void GameConnectionManager::SetAutoReconnectDelay(float seconds)
		{
			if (mClient != nullptr)
				mClient->SetReconnectDelay(seconds);
		}

		void GameConnectionManager::SetAutoReconnectMaxAttempts(int maxAttempts)
		{
			if (mClient != nullptr)
				mClient->SetReconnectMaxAttempts(maxAttempts);
		}

		void GameConnectionManager::Connect(const char* host, int port)
		{
			DIA_ASSERT(mClient != nullptr, "GameConnectionManager: must call Initialize() before Connect()");
			strncpy_s(mHost, sizeof(mHost), host, _TRUNCATE);
			mPort = port;

			char url[256];
			snprintf(url, sizeof(url), "ws://%s:%d", host, port);
			DIA_LOG_INFO("Editor", "GameConnectionManager: Connect to %s", url);
			mClient->Connect(url);
		}

		void GameConnectionManager::Disconnect()
		{
			DIA_LOG_INFO("Editor", "GameConnectionManager: Disconnect requested");
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
			DIA_LOG_INFO("Editor", "GameConnectionManager: Subscribe topic='%s'", topic.AsChar());
			Subscription sub;
			sub.topic = topic;
			sub.callback = callback;
			mSubscriptions.Add(sub);
		}

		void GameConnectionManager::Unsubscribe(const Dia::Core::StringCRC& topic)
		{
			DIA_LOG_INFO("Editor", "GameConnectionManager: Unsubscribe topic='%s'", topic.AsChar());
			for (unsigned int i = 0; i < mSubscriptions.Size(); ++i)
			{
				if (mSubscriptions[i].topic == topic)
				{
					mSubscriptions.RemoveAt(i);
					return;
				}
			}
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

		void GameConnectionManager::SendCommand(const char* command, const Json::Value& args)
		{
			if (!IsConnected())
				return;

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
			auto* cmd = msg.mutable_command_request();
			cmd->set_command(command);
			Dia::Proto::JsonValueToProtoStruct(args, cmd->mutable_payload());

			char buffer[4096];
			if (Dia::Proto::ToJson(msg, buffer, sizeof(buffer)))
				mClient->SendText(buffer);
		}

		void GameConnectionManager::SendCommandWithResponse(const char* command, const Json::Value& args, CommandResponseCallback callback)
		{
			SendCommand(command, args);

			DIA_ASSERT(!mPendingCommands.IsFull(), "GameConnectionManager: max pending command capacity reached");
			PendingCommand pending;
			pending.command = Dia::Core::StringCRC(command);
			pending.callback = callback;
			mPendingCommands.Add(pending);
		}

		void GameConnectionManager::ProcessCommandResponse(const Json::Value& json)
		{
			if (!json.isMember("command") || !json["command"].isString())
				return;

			Dia::Core::StringCRC command(json["command"].asCString());
			bool success = json.get("success", false).asBool();

			for (unsigned int i = 0; i < mPendingCommands.Size(); ++i)
			{
				if (mPendingCommands[i].command == command)
				{
					mPendingCommands[i].callback(success, json);
					mPendingCommands.RemoveAt(i);
					return;
				}
			}
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
				DIA_LOG_WARNING("Editor", "GameConnectionManager: SendRaw skipped, not connected");
				return;
			}
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string text = Json::writeString(writer, message);
			DIA_LOG_DEBUG("Editor", "GameConnectionManager: SendRaw %u bytes", static_cast<unsigned>(text.size()));
			mClient->SendText(text.c_str());
		}

		void GameConnectionManager::SendRawText(const char* text)
		{
			if (!IsConnected())
			{
				DIA_LOG_WARNING("Editor", "GameConnectionManager: SendRawText skipped, not connected");
				return;
			}
			DIA_LOG_DEBUG("Editor", "GameConnectionManager: SendRawText");
			mClient->SendText(text);
		}

		void GameConnectionManager::HandleMessage(const char* text, unsigned int length)
		{
			Json::Value envelope;
			Json::Reader reader;
			if (!reader.parse(text, text + length, envelope))
				return;

			if (mRawMessageCallback)
				mRawMessageCallback(text, length, envelope);

			dia::debug::DebugMessage protoMsg;
			if (Dia::Proto::FromJson(text, &protoMsg))
			{
				if (protoMsg.payload_case() == dia::debug::DebugMessage::kCommandResponse)
				{
					const auto& resp = protoMsg.command_response();
					Json::Value responseJson;
					responseJson["command"] = resp.command();
					responseJson["success"] = resp.success();
					responseJson["message"] = resp.message();
					if (resp.has_payload())
						responseJson["payload"] = Dia::Proto::ProtoStructToJsonValue(resp.payload());
					ProcessCommandResponse(responseJson);
				}
			}

			if (envelope.isMember("topic") && envelope["topic"].isString())
			{
				Dia::Core::StringCRC topic(envelope["topic"].asCString());
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
			DIA_LOG_INFO("Editor", "GameConnectionManager: HandleConnection connected=%d", connected ? 1 : 0);
			if (connected)
				mLastError[0] = '\0';
			if (mConnectionCallback)
				mConnectionCallback(connected);
		}
	}
}
