#include "JsonDiaStageSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>

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
