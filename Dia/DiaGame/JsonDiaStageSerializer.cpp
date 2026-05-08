#include "JsonDiaStageSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
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

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonDiaStageSerializer::Save(const DiaStageManifest& manifest, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root(Json::objectValue);
			root["name"] = manifest.name.AsCStr();
			root["manifest"] = manifest.manifestPath.AsCStr();

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
