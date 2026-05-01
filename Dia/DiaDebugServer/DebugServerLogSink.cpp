#include "DiaDebugServer/DebugServerLogSink.h"

#include <DiaLogger/LogLevel.h>
#include <DiaWebSocket/Server.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>

namespace Dia
{
	namespace DebugServer
	{
		DebugServerLogSink::DebugServerLogSink()
			: mServer(nullptr)
		{
		}

		void DebugServerLogSink::SetServer(Dia::WebSocket::Server* server)
		{
			mServer = server;
		}

		void DebugServerLogSink::OnLogEntry(const Dia::Logger::LogEntry& entry)
		{
			if (mPending.IsFull())
				return;

			BufferedEntry buffered;
			strncpy_s(buffered.level, sizeof(buffered.level),
				Dia::Logger::LogLevelToString(entry.level), _TRUNCATE);
			strncpy_s(buffered.channel, sizeof(buffered.channel),
				entry.channel.AsChar(), _TRUNCATE);
			strncpy_s(buffered.message, sizeof(buffered.message),
				entry.message, _TRUNCATE);

			mPending.Add(buffered);
		}

		void DebugServerLogSink::FlushToServer()
		{
			if (mServer == nullptr || !mServer->IsRunning()) return;
			if (mPending.Size() == 0) return;

			for (unsigned int i = 0; i < mPending.Size(); ++i)
			{
				const BufferedEntry& e = mPending.At(i);

				dia::debug::DebugMessage msg;
				msg.set_type(dia::debug::MESSAGE_TYPE_LOG);
				auto* log = msg.mutable_log();
				log->set_level(e.level);
				log->set_channel(e.channel);
				log->set_message(e.message);

				char buffer[4096];
				if (Dia::Proto::ToJson(msg, buffer, sizeof(buffer)))
					mServer->BroadcastText(buffer);
			}

			mPending.RemoveAll();
		}
	}
}
