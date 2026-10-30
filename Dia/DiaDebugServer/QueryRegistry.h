#ifndef DIA_DEBUG_SERVER_QUERY_REGISTRY_H
#define DIA_DEBUG_SERVER_QUERY_REGISTRY_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

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

			void Register(const Dia::Core::StringCRC& name, QueryHandler handler);
			void Unregister(const Dia::Core::StringCRC& name);
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
			RegisterVeto mVeto = nullptr;
			void* mVetoContext = nullptr;
		};
	}
}

#endif // DIA_DEBUG_SERVER_QUERY_REGISTRY_H
