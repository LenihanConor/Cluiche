#pragma once

#include <DiaLogger/ISink.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace WebSocket { class Server; }

	namespace DebugServer
	{
		class DebugServerLogSink : public Dia::Logger::ISink
		{
		public:
			DebugServerLogSink();

			void SetServer(Dia::WebSocket::Server* server);

			void OnLogEntry(const Dia::Logger::LogEntry& entry) override;
			const char* GetName() const override { return "DebugServerLog"; }

			void FlushToServer();

		private:
			struct BufferedEntry
			{
				char level[16];
				char channel[64];
				char message[1024];
			};

			static const unsigned int kMaxPendingEntries = 128;

			Dia::WebSocket::Server* mServer;
			Dia::Core::Containers::DynamicArrayC<BufferedEntry, kMaxPendingEntries> mPending;
		};
	}
}
