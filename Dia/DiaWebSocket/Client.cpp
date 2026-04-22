#include "DiaWebSocket/Client.h"
#include "DiaWebSocket/Internal/WebSocketppWrapper.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Threading/Mutex.h"
#include <DiaLogger/DiaLog.h>
#include <DiaLogger/Logger.h>

#include <cstring>
#include <string>

namespace Dia
{
	namespace WebSocket
	{
		struct Client::Impl
		{
			Internal::WsppClient mClient;
			Dia::Core::Thread* mWorkerThread = nullptr;

			ConnectionState mState = ConnectionState::kDisconnected;
			std::string mUrl;

			bool mReconnectOnDisconnect = false;
			float mReconnectDelay = 2.0f;
			int mReconnectMaxAttempts = 5;
			int mReconnectAttemptCount = 0;
			float mConnectionTimeout = 10.0f;
			size_t mMaxMessageSize = Internal::kDefaultMaxMessageSize;

			bool mIsRunning = false;
			Internal::WsppClient::connection_ptr mConnection;
			Dia::Core::Mutex mStateMutex;

			MessageCallback mOnMessage;
			ConnectionCallback mOnConnection;
			ErrorCallback mOnError;

			Dia::Core::Containers::DynamicArrayC<Internal::QueuedEvent, 64> mIncomingQueue;
			Dia::Core::Mutex mIncomingMutex;

			Dia::Core::Containers::DynamicArrayC<Internal::OutgoingMessage, 64> mOutgoingQueue;
			Dia::Core::Mutex mOutgoingMutex;

			void OnOpen(Internal::ConnectionHdl hdl)
			{
				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
					mState = ConnectionState::kConnected;
				}
				mReconnectAttemptCount = 0;

				QueueConnectionEvent(true);
			}

			void OnClose(Internal::ConnectionHdl hdl)
			{
				bool shouldReconnect = false;
				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
					mState = ConnectionState::kDisconnected;
					shouldReconnect = mReconnectOnDisconnect && mIsRunning;
				}

				QueueConnectionEvent(false);

				if (shouldReconnect)
				{
					AttemptReconnect();
				}
			}

			void OnMessage(Internal::ConnectionHdl hdl, Internal::WsppClient::message_ptr msg)
			{
				const std::string& payload = msg->get_payload();

				if (payload.size() > mMaxMessageSize)
				{
					QueueError(Internal::MakeError(ErrorCode::kMessageTooLarge, ErrorSeverity::kError, -1, "Message too large: %zu bytes", payload.size()));
					return;
				}

				MessageType msgType = (msg->get_opcode() == websocketpp::frame::opcode::text)
					? MessageType::kText
					: MessageType::kBinary;

				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);

				if (mIncomingQueue.IsFull())
				{
					DIA_LOG_WARNING("WebSocket", "Client: Incoming queue full - dropping message");
					return;
				}

				Internal::QueuedEvent evt;
				evt.eventType = Internal::QueuedEvent::Type::kMessage;
				evt.connectionId = -1;
				evt.message.type = msgType;
				evt.message.connectionId = -1;

				if (msgType == MessageType::kText)
				{
					evt.dataBuffer.Assign(payload.c_str(), static_cast<unsigned int>(payload.size() + 1));
					evt.message.length = payload.size();
				}
				else
				{
					evt.dataBuffer.Assign(payload.data(), static_cast<unsigned int>(payload.size()));
					evt.message.length = payload.size();
				}

				evt.message.data = &evt.dataBuffer.At(0);
				mIncomingQueue.Add(evt);
			}

			void OnFail(Internal::ConnectionHdl hdl)
			{
				bool shouldReconnect = false;
				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
					mState = ConnectionState::kError;
					shouldReconnect = mReconnectOnDisconnect && mIsRunning;
				}

				QueueError(Internal::MakeError(ErrorCode::kConnectionFailed, ErrorSeverity::kError, -1, "Connection failed"));

				if (shouldReconnect)
				{
					AttemptReconnect();
				}
			}

			void QueueConnectionEvent(bool connected)
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);
				if (mIncomingQueue.IsFull()) return;

				Internal::QueuedEvent evt;
				evt.eventType = connected ? Internal::QueuedEvent::Type::kConnected
				                          : Internal::QueuedEvent::Type::kDisconnected;
				evt.connectionId = -1;
				mIncomingQueue.Add(evt);
			}

			void QueueError(const Error& error)
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);
				if (mIncomingQueue.IsFull()) return;

				Internal::QueuedEvent evt;
				evt.eventType = Internal::QueuedEvent::Type::kError;
				evt.connectionId = error.connectionId;
				evt.error = error;
				mIncomingQueue.Add(evt);
			}

			void ProcessOutgoing()
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mOutgoingMutex);

				if (!mConnection) return;

				for (unsigned int i = 0; i < mOutgoingQueue.Size(); ++i)
				{
					const Internal::OutgoingMessage& msg = mOutgoingQueue.At(i);
					websocketpp::frame::opcode::value opcode =
						(msg.type == MessageType::kText)
						? websocketpp::frame::opcode::text
						: websocketpp::frame::opcode::binary;

					try
					{
						mClient.send(mConnection,
							&msg.dataBuffer.At(0),
							msg.dataLength,
							opcode);
					}
					catch (const std::exception& e)
					{
						DIA_LOG_ERROR("WebSocket", "Client: Send failed: %s", e.what());
					}
				}

				mOutgoingQueue.RemoveAll();
			}

			void AttemptReconnect()
			{
				if (mReconnectAttemptCount >= mReconnectMaxAttempts)
				{
					DIA_LOG_WARNING("WebSocket", "Client: Max reconnect attempts reached");
					QueueError(Internal::MakeError(ErrorCode::kReconnectExhausted, ErrorSeverity::kError, -1, "Max reconnect attempts reached"));
					return;
				}

				mReconnectAttemptCount++;

				Dia::Core::ThisThread::SleepMs(static_cast<unsigned int>(mReconnectDelay * 1000));

				if (!mIsRunning) return;

				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
					mState = ConnectionState::kConnecting;
				}

				QueueConnectionEvent(false);

				try
				{
					websocketpp::lib::error_code ec;
					mConnection = mClient.get_connection(mUrl, ec);

					if (ec)
					{
						Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
						mState = ConnectionState::kError;
						return;
					}

					mClient.connect(mConnection);
				}
				catch (const std::exception& e)
				{
					DIA_LOG_ERROR("WebSocket", "Client: Reconnect exception: %s", e.what());
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mStateMutex);
					mState = ConnectionState::kError;
				}
			}

			void WorkerThreadMain()
			{
				Dia::Logger::Logger::Instance().RegisterThreadBuffer();
				DIA_LOG_INFO("WebSocket", "Client worker thread registered for logging");

				while (mIsRunning)
				{
					try
					{
						mClient.poll();
						ProcessOutgoing();
					}
					catch (const std::exception& e)
					{
						DIA_LOG_ERROR("WebSocket", "Client: Worker error: %s", e.what());
					}
					catch (...)
					{
						DIA_LOG_ERROR("WebSocket", "Client: Worker unknown error");
					}

					Dia::Core::ThisThread::SleepMs(1);
				}

				Dia::Logger::Logger::Instance().UnregisterThreadBuffer();
			}
		};

		Client::Client()
			: mImpl(new Impl())
		{
		}

		Client::~Client()
		{
			if (mImpl->mIsRunning)
			{
				Disconnect();
			}
			delete mImpl;
		}

		bool Client::Connect(const char* url)
		{
			ConnectionState currentState = GetState();
			if (currentState == ConnectionState::kConnected)
			{
				return true;
			}
			if (currentState == ConnectionState::kConnecting)
			{
				return false;
			}

			mImpl->mUrl = url;
			mImpl->mReconnectAttemptCount = 0;

			try
			{
				mImpl->mClient.clear_access_channels(websocketpp::log::alevel::all);
				mImpl->mClient.clear_error_channels(websocketpp::log::elevel::all);

				mImpl->mClient.init_asio();

				mImpl->mClient.set_open_handler([this](Internal::ConnectionHdl hdl) {
					mImpl->OnOpen(hdl);
				});
				mImpl->mClient.set_close_handler([this](Internal::ConnectionHdl hdl) {
					mImpl->OnClose(hdl);
				});
				mImpl->mClient.set_message_handler([this](Internal::ConnectionHdl hdl, Internal::WsppClient::message_ptr msg) {
					mImpl->OnMessage(hdl, msg);
				});
				mImpl->mClient.set_fail_handler([this](Internal::ConnectionHdl hdl) {
					mImpl->OnFail(hdl);
				});

				websocketpp::lib::error_code ec;
				mImpl->mConnection = mImpl->mClient.get_connection(url, ec);

				if (ec)
				{
					DIA_LOG_ERROR("WebSocket", "Client: Connection failed: %s", ec.message().c_str());
					mImpl->mState = ConnectionState::kError;
					return false;
				}

				mImpl->mClient.connect(mImpl->mConnection);
			}
			catch (const std::exception& e)
			{
				DIA_LOG_ERROR("WebSocket", "Client: Connection exception: %s", e.what());
				mImpl->mState = ConnectionState::kError;
				return false;
			}

			mImpl->mIsRunning = true;
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mStateMutex);
				mImpl->mState = ConnectionState::kConnecting;
			}

			mImpl->mWorkerThread = new Dia::Core::Thread("WebSocket Client", [this]() {
				mImpl->WorkerThreadMain();
			});

			float elapsed = 0.0f;
			while (GetState() == ConnectionState::kConnecting && elapsed < mImpl->mConnectionTimeout)
			{
				Dia::Core::ThisThread::SleepMs(100);
				elapsed += 0.1f;
			}

			if (GetState() == ConnectionState::kConnected)
			{
				return true;
			}
			else
			{
				DIA_LOG_WARNING("WebSocket", "Client: Connection timeout after %.1fs", elapsed);
				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mStateMutex);
					if (mImpl->mState == ConnectionState::kConnecting)
					{
						mImpl->mState = ConnectionState::kError;
					}
				}
				return false;
			}
		}

		void Client::Disconnect()
		{
			if (GetState() == ConnectionState::kDisconnected && !mImpl->mIsRunning) return;

			bool wasAutoReconnect = mImpl->mReconnectOnDisconnect;
			mImpl->mReconnectOnDisconnect = false;

			if (mImpl->mConnection)
			{
				try
				{
					mImpl->mClient.close(mImpl->mConnection,
						websocketpp::close::status::normal,
						"Client disconnecting");
				}
				catch (const std::exception& e)
				{
					DIA_LOG_ERROR("WebSocket", "Client: Disconnect error: %s", e.what());
				}
			}

			mImpl->mIsRunning = false;

			try
			{
				mImpl->mClient.stop();
			}
			catch (...)
			{
			}

			if (mImpl->mWorkerThread)
			{
				mImpl->mWorkerThread->Join();
				delete mImpl->mWorkerThread;
				mImpl->mWorkerThread = nullptr;
			}

			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mStateMutex);
				mImpl->mState = ConnectionState::kDisconnected;
			}

			mImpl->mReconnectOnDisconnect = wasAutoReconnect;

			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);
				mImpl->mIncomingQueue.RemoveAll();
			}
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);
				mImpl->mOutgoingQueue.RemoveAll();
			}
		}

		bool Client::IsConnected() const
		{
			return GetState() == ConnectionState::kConnected;
		}

		ConnectionState Client::GetState() const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mStateMutex);
			return mImpl->mState;
		}

		void Client::Update()
		{
			if (!mImpl->mIsRunning) return;

			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);

			for (unsigned int i = 0; i < mImpl->mIncomingQueue.Size(); ++i)
			{
				Internal::QueuedEvent& evt = mImpl->mIncomingQueue.At(i);

				switch (evt.eventType)
				{
				case Internal::QueuedEvent::Type::kMessage:
					evt.message.data = &evt.dataBuffer.At(0);
					if (mImpl->mOnMessage)
					{
						mImpl->mOnMessage(evt.message);
					}
					break;

				case Internal::QueuedEvent::Type::kConnected:
					if (mImpl->mOnConnection)
					{
						mImpl->mOnConnection(true);
					}
					break;

				case Internal::QueuedEvent::Type::kDisconnected:
					if (mImpl->mOnConnection)
					{
						mImpl->mOnConnection(false);
					}
					break;

				case Internal::QueuedEvent::Type::kError:
					if (mImpl->mOnError)
					{
						mImpl->mOnError(evt.error);
					}
					break;
				}
			}

			mImpl->mIncomingQueue.RemoveAll();
		}

		void Client::Send(const void* data, size_t length, MessageType type)
		{
			if (length > mImpl->mMaxMessageSize)
			{
				DIA_LOG_WARNING("WebSocket", "Client: Send message too large");
				return;
			}

			Internal::OutgoingMessage msg;
			msg.connectionId = -1;
			msg.type = type;

			if (type == MessageType::kText)
			{
				msg.dataBuffer.Assign(static_cast<const char*>(data), static_cast<unsigned int>(length + 1));
				msg.dataBuffer.At(static_cast<unsigned int>(length)) = '\0';
				msg.dataLength = length;
			}
			else
			{
				msg.dataBuffer.Assign(static_cast<const char*>(data), static_cast<unsigned int>(length));
				msg.dataLength = length;
			}

			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);

			if (mImpl->mOutgoingQueue.IsFull())
			{
				DIA_LOG_WARNING("WebSocket", "Client: Outgoing queue full - dropping message");
				return;
			}

			mImpl->mOutgoingQueue.Add(msg);
		}

		void Client::SendText(const char* text)
		{
			Send(text, strlen(text), MessageType::kText);
		}

		void Client::SendBinary(const void* data, size_t length)
		{
			Send(data, length, MessageType::kBinary);
		}

		void Client::SetReconnectOnDisconnect(bool enable)
		{
			mImpl->mReconnectOnDisconnect = enable;
		}

		void Client::SetReconnectDelay(float seconds)
		{
			mImpl->mReconnectDelay = seconds;
		}

		void Client::SetReconnectMaxAttempts(int maxAttempts)
		{
			mImpl->mReconnectMaxAttempts = maxAttempts;
		}

		void Client::SetConnectionTimeout(float seconds)
		{
			mImpl->mConnectionTimeout = seconds;
		}

		void Client::SetMaxMessageSize(size_t bytes)
		{
			mImpl->mMaxMessageSize = bytes;
		}

		void Client::SetMessageCallback(MessageCallback callback)
		{
			mImpl->mOnMessage = callback;
		}

		void Client::SetConnectionCallback(ConnectionCallback callback)
		{
			mImpl->mOnConnection = callback;
		}

		void Client::SetErrorCallback(ErrorCallback callback)
		{
			mImpl->mOnError = callback;
		}
	}
}
