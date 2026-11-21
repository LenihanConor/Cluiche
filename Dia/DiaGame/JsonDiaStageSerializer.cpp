#include "JsonDiaStageSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>
#include <string>
#include <vector>

namespace Dia
{
	namespace Game
	{
		static const char* kSchemaVersion = "1.0";

		const char* JsonDiaStageSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		Dia::Serializer::SerializeResult JsonDiaStageSerializer::Load(const char* data, DiaStageManifest& outManifest) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
				return Dia::Serializer::SerializeResult::Failure("json parse error");

			if (!root.isMember("name") || !root["name"].isString())
				return Dia::Serializer::SerializeResult::Failure("missing required field: name");

			if (!root.isMember("manifest") || !root["manifest"].isString())
				return Dia::Serializer::SerializeResult::Failure("missing required field: manifest");

			outManifest.name = root["name"].asCString();
			outManifest.manifestPath = root["manifest"].asCString();

			if (root.isMember("scene") && root["scene"].isString())
				outManifest.scenePath = root["scene"].asCString();

			outManifest.tags.RemoveAll();
			outManifest.description = Dia::Core::Containers::String128();

			if (root.isMember("tags"))
			{
				const Json::Value& tags = root["tags"];
				if (!tags.isArray())
					return Dia::Serializer::SerializeResult::Failure("field 'tags' must be an array of strings");
				if (tags.size() > outManifest.tags.Capacity())
					return Dia::Serializer::SerializeResult::Failure("field 'tags' has too many entries (max 16)");
				for (Json::ArrayIndex i = 0; i < tags.size(); ++i)
				{
					if (!tags[i].isString())
						return Dia::Serializer::SerializeResult::Failure("field 'tags' must contain only strings");
					if (strlen(tags[i].asCString()) >= Dia::Core::Containers::String64::Capacity())
						return Dia::Serializer::SerializeResult::Failure("field 'tags' has an entry that is too long (max 63 chars)");
					Dia::Core::Containers::String64 tag("%s", tags[i].asCString());
					outManifest.tags.Add(tag);
				}
			}

			outManifest.proves.RemoveAll();
			if (root.isMember("proves"))
			{
				const Json::Value& proves = root["proves"];
				if (!proves.isArray())
					return Dia::Serializer::SerializeResult::Failure("field 'proves' must be an array of strings");
				if (proves.size() > outManifest.proves.Capacity())
					return Dia::Serializer::SerializeResult::Failure("field 'proves' has too many entries (max 16)");
				for (Json::ArrayIndex i = 0; i < proves.size(); ++i)
				{
					if (!proves[i].isString())
						return Dia::Serializer::SerializeResult::Failure("field 'proves' must contain only strings");
					if (strlen(proves[i].asCString()) >= Dia::Core::Containers::String256::Capacity())
						return Dia::Serializer::SerializeResult::Failure("field 'proves' has an entry that is too long (max 255 chars)");
					Dia::Core::Containers::String256 path("%s", proves[i].asCString());
					outManifest.proves.Add(path);
				}
			}

			outManifest.presets.RemoveAll();
			if (root.isMember("presets"))
			{
				const Json::Value& presets = root["presets"];
				if (!presets.isArray())
					return Dia::Serializer::SerializeResult::Failure("field 'presets' must be an array of objects");
				if (presets.size() > outManifest.presets.Capacity())
					return Dia::Serializer::SerializeResult::Failure("field 'presets' has too many entries (max 16)");
				for (Json::ArrayIndex i = 0; i < presets.size(); ++i)
				{
					const Json::Value& p = presets[i];
					if (!p.isObject())
						return Dia::Serializer::SerializeResult::Failure("field 'presets' must contain only objects");
					if (!p.isMember("name") || !p["name"].isString())
						return Dia::Serializer::SerializeResult::Failure("field 'presets' entry is missing required string 'name'");
					if (strlen(p["name"].asCString()) >= Dia::Core::Containers::String32::Capacity())
						return Dia::Serializer::SerializeResult::Failure("field 'presets' entry 'name' is too long (max 31 chars)");

					DiaStagePreset preset;
					preset.name = Dia::Core::Containers::String32("%s", p["name"].asCString());

					if (p.isMember("description"))
					{
						if (!p["description"].isString())
							return Dia::Serializer::SerializeResult::Failure("field 'presets' entry 'description' must be a string");
						if (strlen(p["description"].asCString()) > 120)
							return Dia::Serializer::SerializeResult::Failure("field 'presets' entry 'description' is too long (max 120 chars)");
						preset.description = Dia::Core::Containers::String128("%s", p["description"].asCString());
					}

					if (p.isMember("modules"))
					{
						const Json::Value& modules = p["modules"];
						if (!modules.isObject())
							return Dia::Serializer::SerializeResult::Failure("field 'presets' entry 'modules' must be an object");
						if (modules.size() > preset.modules.Capacity())
							return Dia::Serializer::SerializeResult::Failure("field 'presets' entry has too many modules (max 4)");
						for (Json::Value::const_iterator it = modules.begin(); it != modules.end(); ++it)
						{
							const std::string id = it.key().asString();
							if (!(*it).isObject())
								return Dia::Serializer::SerializeResult::Failure("field 'presets' module override must be an object");
							if (id.size() >= Dia::Core::Containers::String64::Capacity())
								return Dia::Serializer::SerializeResult::Failure("field 'presets' module instance id is too long (max 63 chars)");
							Json::FastWriter fw;
							std::string text = fw.write(*it);
							while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
								text.pop_back();
							if (text.size() > DiaStagePresetModule::kMaxOverrideBytes)
								return Dia::Serializer::SerializeResult::Failure("field 'presets' module override is too large (max 2048 bytes)");
							DiaStagePresetModule mod;
							mod.instanceId = Dia::Core::Containers::String64("%s", id.c_str());
							memcpy(mod.overridesJson, text.c_str(), text.size() + 1);
							preset.modules.Add(mod);
						}
					}
					outManifest.presets.Add(preset);
				}
			}

			if (root.isMember("description"))
			{
				if (!root["description"].isString())
					return Dia::Serializer::SerializeResult::Failure("field 'description' must be a string");
				if (strlen(root["description"].asCString()) > 120)
					return Dia::Serializer::SerializeResult::Failure("field 'description' is too long (max 120 chars)");
				outManifest.description = Dia::Core::Containers::String128("%s", root["description"].asCString());
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonDiaStageSerializer::Save(const DiaStageManifest& manifest, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root(Json::objectValue);
			root["name"] = manifest.name.AsCStr();
			root["manifest"] = manifest.manifestPath.AsCStr();
			if (manifest.scenePath.Length() > 0)
				root["scene"] = manifest.scenePath.AsCStr();

			if (manifest.tags.Size() > 0)
			{
				Json::Value tags(Json::arrayValue);
				for (unsigned int i = 0; i < manifest.tags.Size(); ++i)
					tags.append(manifest.tags[i].AsCStr());
				root["tags"] = tags;
			}
			if (manifest.proves.Size() > 0)
			{
				Json::Value proves(Json::arrayValue);
				for (unsigned int i = 0; i < manifest.proves.Size(); ++i)
					proves.append(manifest.proves[i].AsCStr());
				root["proves"] = proves;
			}
			if (manifest.presets.Size() > 0)
			{
				Json::Value presets(Json::arrayValue);
				for (unsigned int i = 0; i < manifest.presets.Size(); ++i)
				{
					const DiaStagePreset& preset = manifest.presets[i];
					Json::Value p(Json::objectValue);
					p["name"] = preset.name.AsCStr();
					if (preset.description.Length() > 0)
						p["description"] = preset.description.AsCStr();
					Json::Value modules(Json::objectValue);
					for (unsigned int m = 0; m < preset.modules.Size(); ++m)
					{
						Json::Value overrides;
						Json::Reader reader;
						if (!reader.parse(preset.modules[m].overridesJson, overrides) || !overrides.isObject())
							return Dia::Serializer::SerializeResult::Failure("field 'presets' module override is not a valid JSON object");
						modules[preset.modules[m].instanceId.AsCStr()] = overrides;
					}
					p["modules"] = modules;
					presets.append(p);
				}
				root["presets"] = presets;
			}
			if (manifest.description.Length() > 0)
				root["description"] = manifest.description.AsCStr();

			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonDiaStageSerializer::LoadFromFile(const char* path, DiaStageManifest& outManifest) const
		{
			static const unsigned int kBufferSize = 4096;
			char buffer[kBufferSize];

			if (!ReadFileToBuffer(path, buffer, kBufferSize))
				return Dia::Serializer::SerializeResult::Failure("file read error");

			return Load(buffer, outManifest);
		}

		Dia::Serializer::SerializeResult JsonDiaStageSerializer::SaveToFile(const char* path, const DiaStageManifest& manifest) const
		{
			char buffer[4096];
			auto result = Save(manifest, buffer, sizeof(buffer));
			if (!result)
				return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
