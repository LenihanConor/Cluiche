#include "DiaWebSocket/Server.h"
#include "DiaWebSocket/Internal/WebSocketppWrapper.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Threading/Mutex.h"
#include <DiaLogger/DiaLog.h>
#include <DiaLogger/Logger.h>

#include <map>
#include <cstring>

namespace Dia
{
	namespace WebSocket
	{
		struct Server::Impl
		{
			Internal::WsppServer mServer;
			Dia::Core::Thread* mWorkerThread = nullptr;

			uint16_t mPort;
			bool mIsRunning = false;
			int mMaxConnections = Internal::kDefaultMaxConnections;
			size_t mMaxMessageSize = Internal::kDefaultMaxMessageSize;

			std::map<int, Internal::ConnectionHdl, std::owner_less<Internal::ConnectionHdl>> mConnections;
			std::map<int, Internal::ConnectionHdl> mConnectionsById;
			std::map<void*, int> mConnectionIdByPtr;
			int mNextConnectionId = 1;
			Dia::Core::Mutex mConnectionsMutex;

			MessageCallback mOnMessage;
			ConnectionCallback mOnConnection;
			ErrorCallback mOnError;

			Dia::Core::Containers::DynamicArrayC<Internal::QueuedEvent, 64> mIncomingQueue;
			Dia::Core::Mutex mIncomingMutex;

			Dia::Core::Containers::DynamicArrayC<Internal::OutgoingMessage, 64> mOutgoingQueue;
			Dia::Core::Mutex mOutgoingMutex;

			int FindConnectionId(Internal::ConnectionHdl hdl)
			{
				try
				{
					void* ptr = hdl.lock().get();
					auto it = mConnectionIdByPtr.find(ptr);
					if (it != mConnectionIdByPtr.end())
					{
						return it->second;
					}
				}
				catch (...)
				{
				}
				return -1;
			}

			void OnOpen(Internal::ConnectionHdl hdl)
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mConnectionsMutex);

				if (static_cast<int>(mConnectionsById.size()) >= mMaxConnections)
				{
					try
					{
						mServer.close(hdl, websocketpp::close::status::try_again_later, "Server full");
					}
					catch (...)
					{
					}

					QueueError(Internal::MakeError(ErrorCode::kConnectionRejected, ErrorSeverity::kWarning, -1, "Connection rejected: server full"));
					return;
				}

				int connId = mNextConnectionId++;
				mConnectionsById[connId] = hdl;
				try
				{
					mConnectionIdByPtr[hdl.lock().get()] = connId;
				}
				catch (...)
				{
				}

				QueueConnectionEvent(connId, true);
			}

			void OnClose(Internal::ConnectionHdl hdl)
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mConnectionsMutex);

				int connId = FindConnectionId(hdl);
				if (connId == -1) return;

				try
				{
					mConnectionIdByPtr.erase(hdl.lock().get());
				}
				catch (...)
				{
				}
				mConnectionsById.erase(connId);

				QueueConnectionEvent(connId, false);
			}

			void OnMessage(Internal::ConnectionHdl hdl, Internal::WsppServer::message_ptr msg)
			{
				int connId;
				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mConnectionsMutex);
					connId = FindConnectionId(hdl);
				}

				if (connId == -1) return;

				const std::string& payload = msg->get_payload();

				if (payload.size() > mMaxMessageSize)
				{
					QueueError(Internal::MakeError(ErrorCode::kMessageTooLarge, ErrorSeverity::kError, connId, "Message too large: %zu bytes", payload.size()));
					return;
				}

				MessageType msgType = (msg->get_opcode() == websocketpp::frame::opcode::text)
					? MessageType::kText
					: MessageType::kBinary;

				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);

				if (mIncomingQueue.IsFull())
				{
					DIA_LOG_WARNING("WebSocket", "Server: Incoming queue full - dropping message");
					return;
				}

				Internal::QueuedEvent evt;
				evt.eventType = Internal::QueuedEvent::Type::kMessage;
				evt.connectionId = connId;
				evt.message.type = msgType;
				evt.message.connectionId = connId;

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
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mConnectionsMutex);
				int connId = FindConnectionId(hdl);
				QueueError(Internal::MakeError(ErrorCode::kConnectionFailed, ErrorSeverity::kError, connId, "Connection failed"));
			}

			void QueueConnectionEvent(int connId, bool connected)
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mIncomingMutex);
				if (mIncomingQueue.IsFull()) return;

				Internal::QueuedEvent evt;
				evt.eventType = connected ? Internal::QueuedEvent::Type::kConnected
				                          : Internal::QueuedEvent::Type::kDisconnected;
				evt.connectionId = connId;
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
				Dia::Core::ScopedLock<Dia::Core::Mutex> outLock(mOutgoingMutex);
				Dia::Core::ScopedLock<Dia::Core::Mutex> connLock(mConnectionsMutex);

				for (unsigned int i = 0; i < mOutgoingQueue.Size(); ++i)
				{
					const Internal::OutgoingMessage& msg = mOutgoingQueue.At(i);
					websocketpp::frame::opcode::value opcode =
						(msg.type == MessageType::kText)
						? websocketpp::frame::opcode::text
						: websocketpp::frame::opcode::binary;

					if (msg.connectionId == -1)
					{
						for (auto& pair : mConnectionsById)
						{
							try
							{
								mServer.send(pair.second,
									&msg.dataBuffer.At(0),
									msg.dataLength,
									opcode);
							}
							catch (const std::exception& e)
							{
								DIA_LOG_ERROR("WebSocket", "Server: Send failed: %s", e.what());
							}
						}
					}
					else
					{
						auto it = mConnectionsById.find(msg.connectionId);
						if (it != mConnectionsById.end())
						{
							try
							{
								mServer.send(it->second,
									&msg.dataBuffer.At(0),
									msg.dataLength,
									opcode);
							}
							catch (const std::exception& e)
							{
								DIA_LOG_ERROR("WebSocket", "Server: Send failed: %s", e.what());
							}
						}
					}
				}

				mOutgoingQueue.RemoveAll();
			}

			void WorkerThreadMain()
			{
				Dia::Logger::Logger::Instance().RegisterThreadBuffer();
				DIA_LOG_INFO("WebSocket", "Server worker thread registered for logging");

				while (mIsRunning)
				{
					try
					{
						mServer.poll();
						ProcessOutgoing();
					}
					catch (const std::exception& e)
					{
						DIA_LOG_ERROR("WebSocket", "Server: Worker error: %s", e.what());
					}
					catch (...)
					{
						DIA_LOG_ERROR("WebSocket", "Server: Worker unknown error");
					}

					Dia::Core::ThisThread::SleepMs(1);
				}

				Dia::Logger::Logger::Instance().UnregisterThreadBuffer();
			}
		};

		Server::Server(uint16_t port)
			: mImpl(new Impl())
		{
			mImpl->mPort = port;
		}

		Server::~Server()
		{
			if (mImpl->mIsRunning)
			{
				Stop();
			}
			delete mImpl;
		}

		bool Server::Start()
		{
			if (mImpl->mIsRunning) return false;

			try
			{
				mImpl->mServer.clear_access_channels(websocketpp::log::alevel::all);
				mImpl->mServer.clear_error_channels(websocketpp::log::elevel::all);

				mImpl->mServer.init_asio();
				mImpl->mServer.set_reuse_addr(true);

				mImpl->mServer.set_open_handler([this](Internal::ConnectionHdl hdl) {
					mImpl->OnOpen(hdl);
				});
				mImpl->mServer.set_close_handler([this](Internal::ConnectionHdl hdl) {
					mImpl->OnClose(hdl);
				});
				mImpl->mServer.set_message_handler([this](Internal::ConnectionHdl hdl, Internal::WsppServer::message_ptr msg) {
					mImpl->OnMessage(hdl, msg);
				});
				mImpl->mServer.set_fail_handler([this](Internal::ConnectionHdl hdl) {
					mImpl->OnFail(hdl);
				});

				mImpl->mServer.listen(mImpl->mPort);
				mImpl->mServer.start_accept();
			}
			catch (const std::exception& e)
			{
				DIA_LOG_ERROR("WebSocket", "Server: Failed to start on port %d: %s", static_cast<int>(mImpl->mPort), e.what());
				return false;
			}

			mImpl->mIsRunning = true;
			mImpl->mWorkerThread = new Dia::Core::Thread("WebSocket Server", [this]() {
				mImpl->WorkerThreadMain();
			});

			return true;
		}

		void Server::Stop()
		{
			if (!mImpl->mIsRunning) return;

			mImpl->mIsRunning = false;

			try
			{
				mImpl->mServer.stop_listening();

				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mConnectionsMutex);
					for (auto& pair : mImpl->mConnectionsById)
					{
						try
						{
							mImpl->mServer.close(pair.second,
								websocketpp::close::status::going_away,
								"Server shutting down");
						}
						catch (...)
						{
						}
					}
					mImpl->mConnectionsById.clear();
					mImpl->mConnectionIdByPtr.clear();
				}

				mImpl->mServer.stop();
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
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mIncomingMutex);
				mImpl->mIncomingQueue.RemoveAll();
			}
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mOutgoingMutex);
				mImpl->mOutgoingQueue.RemoveAll();
			}
		}

		bool Server::IsRunning() const
		{
			return mImpl->mIsRunning;
		}

		void Server::Update()
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
						mImpl->mOnMessage(evt.connectionId, evt.message);
					}
					break;

				case Internal::QueuedEvent::Type::kConnected:
					if (mImpl->mOnConnection)
					{
						mImpl->mOnConnection(evt.connectionId, true);
					}
					break;

				case Internal::QueuedEvent::Type::kDisconnected:
					if (mImpl->mOnConnection)
					{
						mImpl->mOnConnection(evt.connectionId, false);
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

		void Server::Broadcast(const void* data, size_t length, MessageType type)
		{
			if (length > mImpl->mMaxMessageSize)
			{
				DIA_LOG_WARNING("WebSocket", "Server: Broadcast message too large");
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
				DIA_LOG_WARNING("WebSocket", "Server: Outgoing queue full - dropping message");
				return;
			}

			mImpl->mOutgoingQueue.Add(msg);
		}

		void Server::Send(int connectionId, const void* data, size_t length, MessageType type)
		{
			if (length > mImpl->mMaxMessageSize)
			{
				DIA_LOG_WARNING("WebSocket", "Server: Send message too large");
				return;
			}

			Internal::OutgoingMessage msg;
			msg.connectionId = connectionId;
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
				DIA_LOG_WARNING("WebSocket", "Server: Outgoing queue full - dropping message");
				return;
			}

			mImpl->mOutgoingQueue.Add(msg);
		}

		void Server::BroadcastText(const char* text)
		{
			Broadcast(text, strlen(text), MessageType::kText);
		}

		void Server::SendText(int connectionId, const char* text)
		{
			Send(connectionId, text, strlen(text), MessageType::kText);
		}

		void Server::BroadcastBinary(const void* data, size_t length)
		{
			Broadcast(data, length, MessageType::kBinary);
		}

		void Server::SendBinary(int connectionId, const void* data, size_t length)
		{
			Send(connectionId, data, length, MessageType::kBinary);
		}

		int Server::GetConnectionCount() const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mConnectionsMutex);
			return static_cast<int>(mImpl->mConnectionsById.size());
		}

		Dia::Core::Containers::DynamicArrayC<int, 16> Server::GetActiveConnectionIds() const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mConnectionsMutex);
			Dia::Core::Containers::DynamicArrayC<int, 16> ids;

			for (const auto& pair : mImpl->mConnectionsById)
			{
				if (!ids.IsFull())
				{
					ids.Add(pair.first);
				}
			}

			return ids;
		}

		void Server::CloseConnection(int connectionId)
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mImpl->mConnectionsMutex);
			auto it = mImpl->mConnectionsById.find(connectionId);
			if (it != mImpl->mConnectionsById.end())
			{
				try
				{
					mImpl->mServer.close(it->second,
						websocketpp::close::status::normal,
						"Connection closed by server");
				}
				catch (const std::exception& e)
				{
					DIA_LOG_ERROR("WebSocket", "Server: CloseConnection failed: %s", e.what());
				}
			}
		}

		void Server::SetMaxConnections(int max)
		{
			DIA_ASSERT(!mImpl->mIsRunning, "SetMaxConnections must be called before Start()");
			mImpl->mMaxConnections = max;
		}

		void Server::SetMaxMessageSize(size_t bytes)
		{
			DIA_ASSERT(!mImpl->mIsRunning, "SetMaxMessageSize must be called before Start()");
			mImpl->mMaxMessageSize = bytes;
		}

		void Server::SetMessageCallback(MessageCallback callback)
		{
			mImpl->mOnMessage = callback;
		}

		void Server::SetConnectionCallback(ConnectionCallback callback)
		{
			mImpl->mOnConnection = callback;
		}

		void Server::SetErrorCallback(ErrorCallback callback)
		{
			mImpl->mOnError = callback;
		}
	}
}
