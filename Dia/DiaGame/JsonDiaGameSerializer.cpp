#include "JsonDiaGameSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <cstring>

namespace Dia
{
	namespace Game
	{
		static const char* kSchemaVersion = "1.0";

		const char* JsonDiaGameSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		Dia::Serializer::SerializeResult JsonDiaGameSerializer::Load(const char* data, DiaGameManifest& outManifest) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
				return Dia::Serializer::SerializeResult::Failure("json parse error");

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
						Dia::Application::TypedImport import;
						import.path = importsJson[i]["path"].asCString();
						const std::string typeStr = importsJson[i].get("type", "manifest").asString();
						import.type = (typeStr == "stage")
							? Dia::Application::TypedImport::ImportType::kStage
							: Dia::Application::TypedImport::ImportType::kManifest;
						outManifest.imports.Add(import);
					}
				}
			}

			if (root.isMember("config") && root["config"].isObject())
			{
				const Json::Value& cfg = root["config"];
				if (cfg.isMember("asset_root") && cfg["asset_root"].isString())
					outManifest.config.assetRoot = cfg["asset_root"].asCString();

				delete outManifest.rawConfig;
				outManifest.rawConfig = new Json::Value(cfg);
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonDiaGameSerializer::Save(const DiaGameManifest& manifest, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root(Json::objectValue);
			root["name"] = manifest.name.AsCStr();
			root["version"] = manifest.version.AsCStr();

			Json::Value importsJson(Json::arrayValue);
			for (unsigned int i = 0; i < manifest.imports.Size(); ++i)
			{
				Json::Value entry;
				entry["path"] = manifest.imports[i].path.AsCStr();
				entry["type"] = (manifest.imports[i].type == Dia::Application::TypedImport::ImportType::kStage)
					? "stage" : "manifest";
				importsJson.append(entry);
			}
			root["imports"] = importsJson;

			if (manifest.rawConfig != nullptr)
			{
				root["config"] = *manifest.rawConfig;
			}
			else
			{
				Json::Value configJson(Json::objectValue);
				if (manifest.config.assetRoot.Length() > 0)
					configJson["asset_root"] = manifest.config.assetRoot.AsCStr();
				root["config"] = configJson;
			}

			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonDiaGameSerializer::LoadFromFile(const char* path, DiaGameManifest& outManifest) const
		{
			static const unsigned int kBufferSize = 16384;
			char buffer[kBufferSize];

			if (!ReadFileToBuffer(path, buffer, kBufferSize))
				return Dia::Serializer::SerializeResult::Failure("file read error");

			return Load(buffer, outManifest);
		}

		Dia::Serializer::SerializeResult JsonDiaGameSerializer::SaveToFile(const char* path, const DiaGameManifest& manifest) const
		{
			char buffer[16384];
			auto result = Save(manifest, buffer, sizeof(buffer));
			if (!result)
				return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
