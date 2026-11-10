#include "JsonDiaGameSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
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
				outManifest.name = Dia::Core::Containers::String256("%s", root["name"].asCString());

			if (root.isMember("version") && root["version"].isString())
				outManifest.version = Dia::Core::Containers::String256("%s", root["version"].asCString());

			if (root.isMember("imports") && root["imports"].isArray())
			{
				const Json::Value& importsJson = root["imports"];
				for (unsigned int i = 0; i < importsJson.size() && !outManifest.imports.IsFull(); ++i)
				{
					if (importsJson[i].isObject() && importsJson[i].isMember("path"))
					{
						Dia::Application::TypedImport import;
						import.path = Dia::Core::Containers::String256("%s", importsJson[i]["path"].asCString());
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
					outManifest.config.assetRoot = Dia::Core::Containers::String256("%s", cfg["asset_root"].asCString());
				if (cfg.isMember("asset_catalogue") && cfg["asset_catalogue"].isString())
					outManifest.config.assetCatalogue = Dia::Core::Containers::String256("%s", cfg["asset_catalogue"].asCString());

				delete outManifest.rawConfig;
				outManifest.rawConfig = new Json::Value(cfg);
			}

			if (root.isMember("editor") && root["editor"].isObject()
				&& root["editor"].isMember("manifests") && root["editor"]["manifests"].isArray())
			{
				const Json::Value& manifestsJson = root["editor"]["manifests"];
				for (unsigned int i = 0; i < manifestsJson.size(); ++i)
				{
					if (!manifestsJson[i].isString() || manifestsJson[i].asString().empty())
						continue;
					if (outManifest.editorManifests.IsFull())
					{
						DIA_LOG_WARNING("Game", "editor.manifests truncated: max %u entries", outManifest.editorManifests.Size());
						break;
					}
					// "%s" format: String256 construction/assignment from const char* is printf-style, a '%' in a path must not be interpreted.
					outManifest.editorManifests.Add(Dia::Core::Containers::String256("%s", manifestsJson[i].asCString()));
				}
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
				if (manifest.config.assetCatalogue.Length() > 0)
					configJson["asset_catalogue"] = manifest.config.assetCatalogue.AsCStr();
				root["config"] = configJson;
			}

			if (manifest.editorManifests.Size() > 0)
			{
				Json::Value manifestsJson(Json::arrayValue);
				for (unsigned int i = 0; i < manifest.editorManifests.Size(); ++i)
					manifestsJson.append(manifest.editorManifests[i].AsCStr());
				Json::Value editorJson(Json::objectValue);
				editorJson["manifests"] = manifestsJson;
				root["editor"] = editorJson;
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
