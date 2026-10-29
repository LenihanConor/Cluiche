#include <DiaVisualDebugger/Console/DomainStateQuery.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <string>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			Json::Value QueryDomainState(const DiaDebugDomainRegistry& registry, const ISchemaKeyResolver* resolver,
										 const Json::Value& args)
			{
				const bool hasId = args.isObject() && args.isMember("id");
				if (!hasId)
				{
					Json::Value root(Json::objectValue);
					BuildDomainStatesRoot(registry, root, resolver);
					return root;
				}

				Json::Value result(Json::objectValue);
				if (!args["id"].isString())
				{
					result["error"] = "param 'id' must be a string";
					return result;
				}

				const std::string id = args["id"].asString();
				IDebugDomain* domain = registry.FindDomain(Dia::Core::StringCRC(id.c_str()));
				if (domain == nullptr)
				{
					result["error"] = "unknown domain id '" + id + "'";
					return result;
				}

				BuildDomainState(*domain, result, resolver);
				return result;
			}
		}
	}
}

#endif // DIA_DEBUG
