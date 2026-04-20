#include "DiaEditor/EditorManifestLoader.h"

#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <string.h>

namespace Dia
{
	namespace Editor
	{
		bool EditorManifestLoader::Load(const char* manifestPath, PluginCallback callback, void* userData)
		{
			if (manifestPath == nullptr || callback == nullptr)
				return false;

			std::ifstream file(manifestPath);
			if (!file.is_open())
				return false;

			Json::Value root;
			Json::CharReaderBuilder builder;
			std::string errors;
			if (!Json::parseFromStream(builder, file, &root, &errors))
				return false;

			const Json::Value& editor = root["editor"];
			if (!editor.isObject() || !editor.get("enabled", false).asBool())
				return true;

			const Json::Value& plugins = editor["plugins"];
			for (unsigned int i = 0; i < plugins.size(); ++i)
			{
				std::string typeStr = plugins[i].get("type", "").asString();
				if (typeStr.empty())
					continue;

				std::string instanceStr = plugins[i].get("instance_id", typeStr.c_str()).asString();

				PluginEntry entry;
				strncpy_s(entry.typeId, sizeof(entry.typeId), typeStr.c_str(), _TRUNCATE);
				strncpy_s(entry.instanceId, sizeof(entry.instanceId), instanceStr.c_str(), _TRUNCATE);

				callback(entry, userData);
			}

			return true;
		}
	}
}
