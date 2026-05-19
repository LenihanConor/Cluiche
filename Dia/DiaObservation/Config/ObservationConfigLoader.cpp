#include <DiaObservation/Config/ObservationConfigLoader.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <fstream>
#include <sstream>
#include <cstring>

namespace Dia
{
	namespace Observation
	{
		LogLevelConfig ObservationConfigLoader::ParseLevel(const char* str, LogLevelConfig fallback)
		{
			if (str == nullptr || str[0] == '\0')
				return fallback;

			if (strcmp(str, "trace") == 0)   return LogLevelConfig::kTrace;
			if (strcmp(str, "debug") == 0)   return LogLevelConfig::kDebug;
			if (strcmp(str, "info") == 0)    return LogLevelConfig::kInfo;
			if (strcmp(str, "warning") == 0) return LogLevelConfig::kWarning;
			if (strcmp(str, "error") == 0)   return LogLevelConfig::kError;

			DIA_LOG_WARNING("Observation", "ObservationConfigLoader: unknown log level '%s', using fallback", str);
			return fallback;
		}

		bool ObservationConfigLoader::LoadFromString(const char* jsonString, ObservationConfig& config)
		{
			if (jsonString == nullptr || jsonString[0] == '\0')
				return false;

			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(jsonString, root))
			{
				DIA_LOG_WARNING("Observation", "ObservationConfigLoader: failed to parse JSON");
				return false;
			}

			const Json::Value& configBlock = root["config"];
			if (configBlock.isNull())
				return false;

			const Json::Value& observation = configBlock["observation"];
			if (observation.isNull())
				return false;

			// Parse global log level
			if (observation.isMember("log_level") && observation["log_level"].isString())
			{
				config.globalLogLevel = ParseLevel(
					observation["log_level"].asCString(), LogLevelConfig::kDefault);
			}

			// Parse per-channel overrides
			if (observation.isMember("log_channels") && observation["log_channels"].isObject())
			{
				const Json::Value& channels = observation["log_channels"];
				Json::Value::Members members = channels.getMemberNames();
				for (size_t i = 0; i < members.size() && config.channelOverrideCount < 16; ++i)
				{
					const std::string& channelName = members[i];
					const Json::Value& levelVal = channels[channelName];
					if (levelVal.isString())
					{
						LogLevelConfig level = ParseLevel(levelVal.asCString(), config.globalLogLevel);
						config.channelOverrides[config.channelOverrideCount].channel =
							Dia::Core::StringCRC(channelName.c_str());
						config.channelOverrides[config.channelOverrideCount].level = level;
						++config.channelOverrideCount;
					}
				}
			}

			// Parse sink enables
			if (observation.isMember("sinks") && observation["sinks"].isObject())
			{
				const Json::Value& sinks = observation["sinks"];
				if (sinks.isMember("stdout") && sinks["stdout"].isBool())
					config.enableStdOutSink = sinks["stdout"].asBool();
				if (sinks.isMember("debug_output") && sinks["debug_output"].isBool())
					config.enableDebugOutputSink = sinks["debug_output"].asBool();
				if (sinks.isMember("observation_file") && sinks["observation_file"].isBool())
					config.enableObservationFileSink = sinks["observation_file"].asBool();
				if (sinks.isMember("trace_file") && sinks["trace_file"].isBool())
					config.enableTraceFileSink = sinks["trace_file"].asBool();
				if (sinks.isMember("metrics_file") && sinks["metrics_file"].isBool())
					config.enableMetricsFileSink = sinks["metrics_file"].asBool();
			}

			// Parse profiling config
			if (observation.isMember("profile") && observation["profile"].isObject())
			{
				const Json::Value& profile = observation["profile"];

				bool enabled = false;
				if (profile.isMember("enabled") && profile["enabled"].isBool())
					enabled = profile["enabled"].asBool();

				config.profileEnabled = enabled;

				if (enabled && profile.isMember("categories") && profile["categories"].isObject())
				{
					const Json::Value& categories = profile["categories"];
					uint32_t mask = 0;

					auto checkCategory = [&](const char* key, uint32_t bit)
					{
						if (categories.isMember(key) && categories[key].isBool()
							&& categories[key].asBool())
							mask |= bit;
					};

					checkCategory("diaapplicationflow", 1u << 0);
					checkCategory("diagraphics",        1u << 1);
					checkCategory("diastream",          1u << 2);
					checkCategory("diaassetruntime",    1u << 3);
					checkCategory("diaanimation",       1u << 4);

					if (categories.isMember("all") && categories["all"].isBool()
						&& categories["all"].asBool())
						mask = ~0u;

					config.profileCategoryMask = mask;
				}
				else if (!enabled)
				{
					config.profileCategoryMask = 0;
				}
			}

			return true;
		}

		bool ObservationConfigLoader::Load(const char* diagamePath, ObservationConfig& config)
		{
			if (diagamePath == nullptr || diagamePath[0] == '\0')
				return false;

			std::ifstream file(diagamePath);
			if (!file.is_open())
			{
				DIA_LOG_WARNING("Observation",
					"ObservationConfigLoader: could not open file '%s'", diagamePath);
				return false;
			}

			std::stringstream buffer;
			buffer << file.rdbuf();
			std::string contents = buffer.str();

			return LoadFromString(contents.c_str(), config);
		}

	} // namespace Observation
} // namespace Dia
