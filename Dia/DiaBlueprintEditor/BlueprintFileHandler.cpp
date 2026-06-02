#include "DiaBlueprintEditor/BlueprintFileHandler.h"
#include <DiaCore/Json/external/json/json.h>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Dia
{
	namespace BlueprintEditor
	{
		BlueprintFileHandler::BlueprintFileHandler()
		{
			mCurrentPath[0] = '\0';
		}

		bool BlueprintFileHandler::Load(const char* path, Json::Value& outRoot,
		                                char* errorOut, unsigned int errorCapacity)
		{
			if (!path || path[0] == '\0')
			{
				if (errorOut && errorCapacity > 0)
					strncpy_s(errorOut, errorCapacity, "empty path", _TRUNCATE);
				return false;
			}

			std::ifstream file(path);
			if (!file.is_open())
			{
				if (errorOut && errorCapacity > 0)
					strncpy_s(errorOut, errorCapacity, "could not open file", _TRUNCATE);
				return false;
			}

			Json::CharReaderBuilder builder;
			std::string errs;
			if (!Json::parseFromStream(builder, file, &outRoot, &errs))
			{
				if (errorOut && errorCapacity > 0)
					strncpy_s(errorOut, errorCapacity, errs.c_str(), _TRUNCATE);
				return false;
			}

			strncpy_s(mCurrentPath, kCurrentPathLength, path, _TRUNCATE);
			return true;
		}

		bool BlueprintFileHandler::Save(const char* path, const Json::Value& root,
		                                char* errorOut, unsigned int errorCapacity)
		{
			if (!path || path[0] == '\0')
			{
				if (errorOut && errorCapacity > 0)
					strncpy_s(errorOut, errorCapacity, "empty path", _TRUNCATE);
				return false;
			}

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "\t";
			std::string serialized = Json::writeString(builder, root);

			std::ofstream file(path, std::ios::out | std::ios::trunc);
			if (!file.is_open())
			{
				if (errorOut && errorCapacity > 0)
					strncpy_s(errorOut, errorCapacity, "could not open file for writing", _TRUNCATE);
				return false;
			}

			file << serialized;
			return file.good();
		}

		const char* BlueprintFileHandler::TopLevelKeyForExtension(const char* ext)
		{
			if (!ext) return "entity_blueprint";
			if (strcmp(ext, ".diaentity") == 0) return "entity_blueprint";
			if (strcmp(ext, ".diacamera") == 0) return "camera_blueprint";
			if (strcmp(ext, ".dialight")  == 0) return "light_blueprint";
			return "entity_blueprint";
		}
	}
}
