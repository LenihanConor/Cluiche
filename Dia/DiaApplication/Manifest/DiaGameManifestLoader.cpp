#include "DiaGameManifestLoader.h"

#include <DiaCore/Json/external/json/json.h>

#include <cstdio>
#include <cstring>

namespace
{
	bool ReadFileToBuffer(const char* path, char* buffer, unsigned int bufferSize)
	{
		FILE* f = fopen(path, "rb");
		if (!f) return false;
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (size < 0 || static_cast<unsigned int>(size) >= bufferSize) { fclose(f); return false; }
		fread(buffer, 1, static_cast<size_t>(size), f);
		buffer[size] = '\0';
		fclose(f);
		return true;
	}
}

namespace Dia
{
	namespace Application
	{
		ManifestValidationResult DiaGameManifestLoader::LoadGameFile(const char* path, DiaGameManifest& outManifest)
		{
			static const unsigned int kBufferSize = 16384;
			char buffer[kBufferSize];

			if (!ReadFileToBuffer(path, buffer, kBufferSize))
				return ManifestValidationResult::kImportNotFound;

			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(buffer, root))
				return ManifestValidationResult::kInvalidJSON;

			if (root.isMember("name") && root["name"].isString())
				outManifest.name = root["name"].asCString();

			if (root.isMember("version") && root["version"].isString())
				outManifest.version = root["version"].asCString();

			if (root.isMember("imports") && root["imports"].isArray())
			{
				const Json::Value& importsJson = root["imports"];
				for (unsigned int i = 0; i < importsJson.size() && !outManifest.imports.IsFull(); ++i)
				{
					if (importsJson[i].isObject() && importsJson[i].isMember("path"))
					{
						TypedImport import;
						import.path = importsJson[i]["path"].asCString();
						const std::string typeStr = importsJson[i].get("type", "manifest").asString();
						import.type = (typeStr == "stage")
							? TypedImport::ImportType::kStage
							: TypedImport::ImportType::kManifest;
						outManifest.imports.Add(import);
					}
				}
			}

			if (root.isMember("config") && root["config"].isObject())
			{
				const Json::Value& cfg = root["config"];
				if (cfg.isMember("asset_root") && cfg["asset_root"].isString())
					outManifest.config.assetRoot = cfg["asset_root"].asCString();
				if (cfg.isMember("default_level") && cfg["default_level"].isString())
					outManifest.config.defaultLevel = cfg["default_level"].asCString();
			}

			return ManifestValidationResult::kSuccess;
		}

		ManifestValidationResult DiaGameManifestLoader::LoadStageFile(const char* path, DiaStageManifest& outManifest)
		{
			static const unsigned int kBufferSize = 4096;
			char buffer[kBufferSize];

			if (!ReadFileToBuffer(path, buffer, kBufferSize))
				return ManifestValidationResult::kImportNotFound;

			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(buffer, root))
				return ManifestValidationResult::kInvalidJSON;

			if (!root.isMember("name") || !root["name"].isString())
				return ManifestValidationResult::kMissingRequiredField;

			if (!root.isMember("manifest") || !root["manifest"].isString())
				return ManifestValidationResult::kMissingRequiredField;

			outManifest.name = root["name"].asCString();
			outManifest.manifestPath = root["manifest"].asCString();

			return ManifestValidationResult::kSuccess;
		}
	}
}
