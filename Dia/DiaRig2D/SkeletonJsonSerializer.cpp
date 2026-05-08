#include "SkeletonJsonSerializer.h"
#include "ISkeletonSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaSerializer/JsonMetadataHelpers.h>
#include <DiaLogger/DiaLog.h>
#include <cstring>

namespace Dia
{
	namespace Rig2D
	{
		static const char* kSchemaVersion = "1.0";

		const char* JsonSkeletonSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		Dia::Serializer::SerializeResult JsonSkeletonSerializer::Load(const char* data, SkeletonDef& outDef) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: failed to parse JSON");
				return Dia::Serializer::SerializeResult::Failure("json parse error");
			}

			if (!root.isMember("id") || !root["id"].isString())
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: missing or invalid 'id' field");
				return Dia::Serializer::SerializeResult::Failure("missing id field");
			}

			outDef.id = Dia::Core::StringCRC(root["id"].asCString());

			if (!root.isMember("bones") || !root["bones"].isArray())
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: missing or invalid 'bones' array");
				return Dia::Serializer::SerializeResult::Failure("missing bones array");
			}

			const Json::Value& bonesArray = root["bones"];

			for (unsigned int i = 0; i < bonesArray.size(); ++i)
			{
				const Json::Value& boneJson = bonesArray[i];

				if (!boneJson.isMember("name") || !boneJson["name"].isString())
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: bone %u missing 'name'", i);
					return Dia::Serializer::SerializeResult::Failure("bone missing name");
				}

				if (!boneJson.isMember("position") || !boneJson["position"].isArray() || boneJson["position"].size() != 2)
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: bone %u ('%s') missing or invalid 'position'",
						i, boneJson["name"].asCString());
					return Dia::Serializer::SerializeResult::Failure("bone missing position");
				}

				Bone bone;
				bone.name = Dia::Core::StringCRC(boneJson["name"].asCString());

				bone.localPosition.Set(
					boneJson["position"][0u].asFloat(),
					boneJson["position"][1u].asFloat()
				);

				if (boneJson.isMember("rotation"))
					bone.localRotation = boneJson["rotation"].asFloat();

				if (boneJson.isMember("scale") && boneJson["scale"].isArray() && boneJson["scale"].size() == 2)
				{
					bone.localScale.Set(
						boneJson["scale"][0u].asFloat(),
						boneJson["scale"][1u].asFloat()
					);
				}

				if (!boneJson.isMember("parent"))
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: bone %u ('%s') missing 'parent'",
						i, boneJson["name"].asCString());
					return Dia::Serializer::SerializeResult::Failure("bone missing parent");
				}

				const Json::Value& parentVal = boneJson["parent"];
				if (parentVal.isInt())
				{
					bone.parentIndex = parentVal.asInt();
				}
				else if (parentVal.isString())
				{
					const char* parentName = parentVal.asCString();
					bone.parentIndex = -1;

					for (unsigned int j = 0; j < outDef.bones.Size(); ++j)
					{
						if (outDef.bones[j].name == Dia::Core::StringCRC(parentName))
						{
							bone.parentIndex = static_cast<int>(j);
							break;
						}
					}

					if (bone.parentIndex == -1)
					{
						DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: bone %u ('%s') references unknown parent '%s'",
							i, boneJson["name"].asCString(), parentName);
						return Dia::Serializer::SerializeResult::Failure("bone references unknown parent");
					}
				}
				else if (parentVal.isNull())
				{
					bone.parentIndex = -1;
				}
				else
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: bone %u ('%s') has invalid 'parent' type",
						i, boneJson["name"].asCString());
					return Dia::Serializer::SerializeResult::Failure("bone invalid parent type");
				}

				Dia::Serializer::ReadMetadataFromJson(boneJson, bone.metadata);

				outDef.bones.Add(bone);
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonSkeletonSerializer::Save(const SkeletonDef& def, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["id"] = def.id.AsChar();

			Json::Value bonesArray(Json::arrayValue);

			for (unsigned int i = 0; i < def.bones.Size(); ++i)
			{
				const Bone& bone = def.bones[i];
				Json::Value boneJson;

				boneJson["name"] = bone.name.AsChar();

				if (bone.parentIndex == -1)
					boneJson["parent"] = -1;
				else
					boneJson["parent"] = def.bones[bone.parentIndex].name.AsChar();

				Json::Value pos(Json::arrayValue);
				pos.append(bone.localPosition.X());
				pos.append(bone.localPosition.Y());
				boneJson["position"] = pos;

				boneJson["rotation"] = bone.localRotation;

				Json::Value scale(Json::arrayValue);
				scale.append(bone.localScale.X());
				scale.append(bone.localScale.Y());
				boneJson["scale"] = scale;

				Dia::Serializer::WriteMetadataToJson(bone.metadata, boneJson);

				bonesArray.append(boneJson);
			}

			root["bones"] = bonesArray;

			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonSerializer: output buffer too small (%u bytes needed, %u available)",
					static_cast<unsigned int>(output.size() + 1), bufferSize);
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");
			}

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// ISkeletonSerializer LoadFromFile / SaveToFile
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult ISkeletonSerializer::LoadFromFile(const char* path, SkeletonDef& outDef) const
		{
			char buffer[32768];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return Load(buffer, outDef);
		}

		Dia::Serializer::SerializeResult ISkeletonSerializer::SaveToFile(const char* path, const SkeletonDef& def) const
		{
			char buffer[32768];
			auto result = Save(def, buffer, sizeof(buffer));
			if (!result)
				return result;
			unsigned int len = static_cast<unsigned int>(strlen(buffer));
			if (!WriteBufferToFile(path, buffer, len))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
