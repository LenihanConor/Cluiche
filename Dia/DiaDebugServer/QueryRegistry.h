#ifndef DIA_DEBUG_SERVER_QUERY_REGISTRY_H
#define DIA_DEBUG_SERVER_QUERY_REGISTRY_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdint>
#include <functional>

namespace Dia
{
	namespace DebugServer
	{
		class QueryRegistry
		{
		public:
			using QueryHandler = std::function<Json::Value(const Json::Value& args)>;

			// Optional veto consulted by Register: if it returns true the registration is refused
			// (Error log, handler not added). Installed by DebugServer so a name that is already
			// sim-hosted cannot also be registered here. Null (default) disables the check.
			using RegisterVeto = bool (*)(void* context, const Dia::Core::StringCRC& name);
			void SetRegisterVeto(RegisterVeto veto, void* context) { mVeto = veto; mVetoContext = context; }

			// Owner-thread rule (Sim-Registered Queries): once an owner is set, Register/Unregister from any other
			// thread is refused (Error log, metric dia.debugserver.query.wrong_thread_register, table unchanged).
			// Handlers that read SimPU state must register through SimQueryHost instead. No owner (default) = any thread.
			// DebugServer::Start sets the owner to the calling thread; Stop clears it.
			using ThreadId = uint32_t;
			static ThreadId CurrentThreadId();
			void SetOwnerThread(ThreadId id)    { mOwnerThread = id; mHasOwner = true; }
			void ClearOwnerThread()             { mHasOwner = false; }

			// Returns false when refused (veto or wrong thread). Duplicates still assert.
			bool Register(const Dia::Core::StringCRC& name, QueryHandler handler);
			// Returns true if an entry was removed; false if unknown or refused (wrong thread).
			bool Unregister(const Dia::Core::StringCRC& name);
			bool Has(const Dia::Core::StringCRC& name) const;
			Json::Value Execute(const Dia::Core::StringCRC& name, const Json::Value& args) const;

		private:
			struct Entry
			{
				Dia::Core::StringCRC name;
				QueryHandler handler;
			};

			static const unsigned int kMaxHandlers = 64;
			Dia::Core::Containers::DynamicArrayC<Entry, kMaxHandlers> mHandlers;
			bool RejectWrongThread(const char* op, const Dia::Core::StringCRC& name) const;

			ThreadId mOwnerThread = 0;
			bool mHasOwner = false;
			RegisterVeto mVeto = nullptr;
			void* mVetoContext = nullptr;
		};
	}
}

#endif // DIA_DEBUG_SERVER_QUERY_REGISTRY_H
