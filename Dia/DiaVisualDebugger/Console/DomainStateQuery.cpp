#include <DiaVisualDebugger/Console/DomainStateQuery.h>

#ifdef DIA_DEBUG

#include <string>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			bool ParseDomainStateSnapshot(const DomainStateSnapshot& snapshot, Json::Value& root)
			{
				if (snapshot.jsonBytes == 0 || snapshot.jsonBytes > kMaxSnapshotBytes)
					return false;

				Json::Value parsed;
				Json::Reader reader;
				if (!reader.parse(snapshot.json, snapshot.json + snapshot.jsonBytes, parsed, false) || !parsed.isObject())
					return false;

				parsed["simFrame"] = Json::Value(Json::UInt64(snapshot.simFrame));
				root = parsed;
				return true;
			}

			void ApplySchemaVersion(Json::Value& root, const ISchemaKeyResolver* resolver)
			{
				if (resolver == nullptr || !resolver->IsAvailable() || !root.isObject())
					return;
				for (const std::string& name : root.getMemberNames())
				{
					Json::Value& entry = root[name];
					if (entry.isObject() && entry.isMember("controls") && entry["controls"].isArray() && !entry.isMember("schemaVersion"))
						entry["schemaVersion"] = resolver->Version();
				}
			}

			Json::Value QueryDomainState(const Json::Value* root, const Json::Value& args)
			{
				Json::Value result(Json::objectValue);
				if (root == nullptr)
				{
					result["error"] = "no domain snapshot yet";
					return result;
				}

				const bool hasId = args.isObject() && args.isMember("id");
				if (!hasId)
					return *root;

				if (!args["id"].isString())
				{
					result["error"] = "param 'id' must be a string";
					return result;
				}

				const std::string id = args["id"].asString();
				for (const std::string& name : root->getMemberNames())
				{
					const Json::Value& entry = (*root)[name];
					if (entry.isObject() && entry.isMember("_id") && entry["_id"].isString() && entry["_id"].asString() == id)
						return entry;
				}

				result["error"] = "unknown domain id '" + id + "'";
				return result;
			}
		}
	}
}

#endif // DIA_DEBUG
