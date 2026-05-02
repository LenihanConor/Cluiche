#include "SkeletonJson.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <cstring>

namespace Dia
{
	namespace Rig2D
	{
		bool JsonSkeletonLoader::Load(const char* data, SkeletonDef& outDef) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: failed to parse JSON");
				return false;
			}

			if (!root.isMember("id") || !root["id"].isString())
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: missing or invalid 'id' field");
				return false;
			}

			outDef.id = Dia::Core::StringCRC(root["id"].asCString());

			if (!root.isMember("bones") || !root["bones"].isArray())
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: missing or invalid 'bones' array");
				return false;
			}

			const Json::Value& bonesArray = root["bones"];

			for (unsigned int i = 0; i < bonesArray.size(); ++i)
			{
				const Json::Value& boneJson = bonesArray[i];

				if (!boneJson.isMember("name") || !boneJson["name"].isString())
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: bone %u missing 'name'", i);
					return false;
				}

				if (!boneJson.isMember("position") || !boneJson["position"].isArray() || boneJson["position"].size() != 2)
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: bone %u ('%s') missing or invalid 'position'",
						i, boneJson["name"].asCString());
					return false;
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
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: bone %u ('%s') missing 'parent'",
						i, boneJson["name"].asCString());
					return false;
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
						DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: bone %u ('%s') references unknown parent '%s'",
							i, boneJson["name"].asCString(), parentName);
						return false;
					}
				}
				else if (parentVal.isNull())
				{
					bone.parentIndex = -1;
				}
				else
				{
					DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: bone %u ('%s') has invalid 'parent' type",
						i, boneJson["name"].asCString());
					return false;
				}

				if (boneJson.isMember("metadata") && boneJson["metadata"].isObject())
				{
					const Json::Value& meta = boneJson["metadata"];
					auto members = meta.getMemberNames();

					for (unsigned int m = 0; m < members.size(); ++m)
					{
						const std::string& key = members[m];
						const Json::Value& val = meta[key];

						MetadataEntry entry;
						entry.key = Dia::Core::StringCRC(key.c_str());

						if (val.isBool())
						{
							entry.value = MetadataValue::FromBool(val.asBool());
						}
						else if (val.isInt())
						{
							entry.value = MetadataValue::FromInt(val.asInt());
						}
						else if (val.isDouble())
						{
							entry.value = MetadataValue::FromFloat(val.asFloat());
						}
						else if (val.isString())
						{
							entry.value = MetadataValue::FromString(val.asCString());
						}
						else
						{
							DIA_LOG_DEBUG("Rig2D", "JsonSkeletonLoader: bone '%s' metadata key '%s' has unsupported type, skipping",
								boneJson["name"].asCString(), key.c_str());
							continue;
						}

						bone.metadata.Add(entry);
					}
				}

				outDef.bones.Add(bone);
			}

			return true;
		}

		bool JsonSkeletonLoader::Save(const SkeletonDef& def, char* outBuffer, unsigned int bufferSize) const
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
				{
					boneJson["parent"] = -1;
				}
				else
				{
					boneJson["parent"] = def.bones[bone.parentIndex].name.AsChar();
				}

				Json::Value pos(Json::arrayValue);
				pos.append(bone.localPosition.X());
				pos.append(bone.localPosition.Y());
				boneJson["position"] = pos;

				boneJson["rotation"] = bone.localRotation;

				Json::Value scale(Json::arrayValue);
				scale.append(bone.localScale.X());
				scale.append(bone.localScale.Y());
				boneJson["scale"] = scale;

				if (bone.metadata.Size() > 0)
				{
					Json::Value metaJson;
					for (unsigned int m = 0; m < bone.metadata.Size(); ++m)
					{
						const MetadataEntry& entry = bone.metadata[m];
						const char* key = entry.key.AsChar();

						switch (entry.value.type)
						{
						case MetadataValue::kBool:
							metaJson[key] = entry.value.boolVal;
							break;
						case MetadataValue::kInt:
							metaJson[key] = entry.value.intVal;
							break;
						case MetadataValue::kFloat:
							metaJson[key] = entry.value.floatVal;
							break;
						case MetadataValue::kString:
							metaJson[key] = entry.value.stringVal.AsChar();
							break;
						}
					}
					boneJson["metadata"] = metaJson;
				}

				bonesArray.append(boneJson);
			}

			root["bones"] = bonesArray;

			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
			{
				DIA_LOG_WARNING("Rig2D", "JsonSkeletonLoader: output buffer too small (%u bytes needed, %u available)",
					static_cast<unsigned int>(output.size() + 1), bufferSize);
				return false;
			}

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return true;
		}
	}
}
