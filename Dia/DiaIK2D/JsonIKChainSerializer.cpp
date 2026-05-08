#include "JsonIKChainSerializer.h"
#include "IIKChainSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <cstring>

namespace Dia
{
	namespace IK2D
	{
		static const char* kSchemaVersion = "1.0";

		const char* JsonIKChainSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		Dia::Serializer::SerializeResult JsonIKChainSerializer::Load(const char* data, IKChainDef& outDef) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
			{
				DIA_LOG_WARNING("IK2D", "JsonIKChainSerializer: failed to parse JSON");
				return Dia::Serializer::SerializeResult::Failure("json parse error");
			}

			if (!root.isMember("id") || !root["id"].isString())
				return Dia::Serializer::SerializeResult::Failure("missing id");

			outDef.id = Dia::Core::StringCRC(root["id"].asCString());

			if (!root.isMember("start_bone") || !root["start_bone"].isString())
				return Dia::Serializer::SerializeResult::Failure("missing start_bone");

			outDef.startBoneId = Dia::Core::StringCRC(root["start_bone"].asCString());

			if (!root.isMember("end_bone") || !root["end_bone"].isString())
				return Dia::Serializer::SerializeResult::Failure("missing end_bone");

			outDef.endBoneId = Dia::Core::StringCRC(root["end_bone"].asCString());

			outDef.reachWeight   = root.get("reach_weight",    1.0f).asFloat();
			outDef.maxIterations = root.get("max_iterations",  20).asInt();
			outDef.tolerance     = root.get("tolerance",       0.001f).asFloat();

			if (root.isMember("joint_limits") && root["joint_limits"].isArray())
			{
				const Json::Value& limits = root["joint_limits"];
				for (unsigned int i = 0; i < limits.size() && i < kMaxJointLimits; ++i)
				{
					JointLimitDef limit;
					limit.minAngle = limits[i].get("min_angle", -Dia::Maths::PI).asFloat();
					limit.maxAngle = limits[i].get("max_angle",  Dia::Maths::PI).asFloat();
					limit.enabled  = limits[i].get("enabled",   false).asBool();
					outDef.jointLimits.Add(limit);
				}
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonIKChainSerializer::Save(const IKChainDef& def, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["id"]             = def.id.AsChar();
			root["start_bone"]     = def.startBoneId.AsChar();
			root["end_bone"]       = def.endBoneId.AsChar();
			root["reach_weight"]   = def.reachWeight;
			root["max_iterations"] = def.maxIterations;
			root["tolerance"]      = def.tolerance;

			Json::Value limitsArray(Json::arrayValue);
			for (unsigned int i = 0; i < def.jointLimits.Size(); ++i)
			{
				const JointLimitDef& limit = def.jointLimits[i];
				Json::Value limitJson;
				limitJson["min_angle"] = limit.minAngle;
				limitJson["max_angle"] = limit.maxAngle;
				limitJson["enabled"]   = limit.enabled;
				limitsArray.append(limitJson);
			}
			root["joint_limits"] = limitsArray;

			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
			{
				DIA_LOG_WARNING("IK2D", "JsonIKChainSerializer: output buffer too small");
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");
			}

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult IIKChainSerializer::LoadFromFile(const char* path, IKChainDef& outDef) const
		{
			char buffer[8192];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return Load(buffer, outDef);
		}

		Dia::Serializer::SerializeResult IIKChainSerializer::SaveToFile(const char* path, const IKChainDef& def) const
		{
			char buffer[8192];
			auto result = Save(def, buffer, sizeof(buffer));
			if (!result)
				return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
