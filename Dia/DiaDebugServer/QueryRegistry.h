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
		};
	}
}

#endif // DIA_DEBUG_SERVER_QUERY_REGISTRY_H
