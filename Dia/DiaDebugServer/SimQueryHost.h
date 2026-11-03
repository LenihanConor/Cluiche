#ifndef DIA_DEBUG_SERVER_SIM_QUERY_HOST_H
#define DIA_DEBUG_SERVER_SIM_QUERY_HOST_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/SimHostedTypes.h>

#include <functional>
#include <memory>

namespace Dia
{
	namespace DebugServer
	{
		// Where the SimQueryHost pulls requests from (SimPU side of the request stream).
		class ISimRequestSource
		{
		public:
			virtual ~ISimRequestSource() {}

			// Returns false when no request is pending.
			virtual bool TryPop(SimRequestEvent& out) = 0;
		};

		// Where the SimQueryHost posts replies (SimPU side of the reply stream).
		class ISimReplySink
		{
		public:
			virtual ~ISimReplySink() {}

			// Returns false if the stream is full.
			virtual bool PostReply(const SimReplyEvent& reply) = 0;
		};

		// Runs queries/commands that touch SimPU-owned state. Handlers run only on the
		// thread that calls Update(). No locking here; the streams provide synchronisation.
		class SimQueryHost
		{
		public:
			using QueryHandler = std::function<Json::Value(const Json::Value& args)>;

			SimQueryHost();

			void SetRequestSource(ISimRequestSource* source);
			void SetReplySink(ISimReplySink* sink);

			// Help is mandatory: a null/empty description, over-limit info or a null handler returns false,
			// registers nothing and logs an Error naming the name. The info is copied.
			bool RegisterQuery(Dia::Core::StringCRC name, const SimHelpInfo& help, QueryHandler handler);
			bool RegisterCommand(Dia::Core::StringCRC name, const SimHelpInfo& help, QueryHandler handler);
			bool UnregisterQuery(Dia::Core::StringCRC name);
			bool Has(Dia::Core::StringCRC name) const;

			// Stored help for a registered name, or nullptr. The pointer is valid until that name is unregistered.
			const SimHelpStored* GetHelp(Dia::Core::StringCRC name) const;

			// Invoked from Update() for a SimKind::Resync control request (no reply is produced, the
			// handler is never looked up by name). Without a handler such requests are dropped.
			void SetResyncHandler(std::function<void()> handler);

			// Drains all pending requests, runs handlers, posts replies.
			void Update();

		private:
			struct Entry
			{
				Dia::Core::StringCRC name;
				SimKind kind;
				QueryHandler handler;
				std::shared_ptr<SimHelpStored> help;   // heap: SimHelpStored is several KB, 64 entries inline would bloat the host
			};

			static const unsigned int kMaxHandlers = 64;

			bool Register(Dia::Core::StringCRC name, SimKind kind, const SimHelpInfo& help, QueryHandler handler);
			void Handle(const SimRequestEvent& request, SimReplyEvent& reply) const;

			Dia::Core::Containers::DynamicArrayC<Entry, kMaxHandlers> mHandlers;
			ISimRequestSource* mSource;
			ISimReplySink* mSink;
			bool mWarnedFull;
			std::function<void()> mResyncHandler;
		};
	}
}

#endif
