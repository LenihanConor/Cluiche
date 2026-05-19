#include <DiaObservation/Config/ObservationConfigCli.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>

namespace Dia
{
	namespace Observation
	{
		static LogLevelConfig ParseLevelCli(const char* str, LogLevelConfig fallback)
		{
			if (str == nullptr || str[0] == '\0')
				return fallback;

			if (strcmp(str, "trace") == 0)   return LogLevelConfig::kTrace;
			if (strcmp(str, "debug") == 0)   return LogLevelConfig::kDebug;
			if (strcmp(str, "info") == 0)    return LogLevelConfig::kInfo;
			if (strcmp(str, "warning") == 0) return LogLevelConfig::kWarning;
			if (strcmp(str, "error") == 0)   return LogLevelConfig::kError;

			DIA_LOG_WARNING("Observation", "ObservationConfigCli: unknown log level '%s'", str);
			return fallback;
		}

		void ObservationConfigCli::ApplyOverrides(int argc, const char* const* argv, ObservationConfig& config)
		{
			if (argv == nullptr || argc <= 0)
				return;

			const char* kLogLevelPrefix = "--log-level=";
			const size_t kLogLevelPrefixLen = 12; // strlen("--log-level=")

			const char* kLogChannelPrefix = "--log-channel=";
			const size_t kLogChannelPrefixLen = 14; // strlen("--log-channel=")

			const char* kProfileCategoriesPrefix = "--profile-categories=";
			const size_t kProfileCategoriesPrefixLen = 21; // strlen("--profile-categories=")

			for (int i = 1; i < argc; ++i)
			{
				if (argv[i] == nullptr)
					continue;

				// --log-level=<level>
				if (strncmp(argv[i], kLogLevelPrefix, kLogLevelPrefixLen) == 0)
				{
					const char* levelStr = argv[i] + kLogLevelPrefixLen;
					LogLevelConfig parsed = ParseLevelCli(levelStr, config.globalLogLevel);
					if (parsed != config.globalLogLevel)
						config.globalLogLevel = parsed;
					else
						config.globalLogLevel = parsed; // Still assign even if same
					continue;
				}

				// --log-channel=<channel>:<level>
				if (strncmp(argv[i], kLogChannelPrefix, kLogChannelPrefixLen) == 0)
				{
					const char* spec = argv[i] + kLogChannelPrefixLen;
					const char* colon = strchr(spec, ':');
					if (colon == nullptr || colon == spec)
						continue; // malformed, skip

					// Extract channel name
					char channelBuf[64] = {};
					size_t channelLen = static_cast<size_t>(colon - spec);
					if (channelLen >= sizeof(channelBuf))
						channelLen = sizeof(channelBuf) - 1;
					memcpy(channelBuf, spec, channelLen);
					channelBuf[channelLen] = '\0';

					const char* levelStr = colon + 1;
					LogLevelConfig level = ParseLevelCli(levelStr, config.globalLogLevel);

					// Check if channel already exists in overrides
					Dia::Core::StringCRC channelCrc(channelBuf);
					bool found = false;
					for (unsigned int j = 0; j < config.channelOverrideCount; ++j)
					{
						if (config.channelOverrides[j].channel == channelCrc)
						{
							config.channelOverrides[j].level = level;
							found = true;
							break;
						}
					}

					if (!found && config.channelOverrideCount < 16)
					{
						config.channelOverrides[config.channelOverrideCount].channel = channelCrc;
						config.channelOverrides[config.channelOverrideCount].level = level;
						++config.channelOverrideCount;
					}
					continue;
				}

				// --profile-categories=<cat1>,<cat2>,...
				if (strncmp(argv[i], kProfileCategoriesPrefix, kProfileCategoriesPrefixLen) == 0)
				{
					const char* list = argv[i] + kProfileCategoriesPrefixLen;
					uint32_t mask = 0;

					// Walk comma-separated tokens in-place (no heap allocation)
					const char* cursor = list;
					while (*cursor != '\0')
					{
						const char* comma = strchr(cursor, ',');
						size_t tokenLen = (comma != nullptr)
							? static_cast<size_t>(comma - cursor)
							: strlen(cursor);

						char token[64] = {};
						if (tokenLen >= sizeof(token))
							tokenLen = sizeof(token) - 1;
						memcpy(token, cursor, tokenLen);
						token[tokenLen] = '\0';

						if      (strcmp(token, "diaapplicationflow") == 0) mask |= 1u << 0;
						else if (strcmp(token, "diagraphics")        == 0) mask |= 1u << 1;
						else if (strcmp(token, "diastream")          == 0) mask |= 1u << 2;
						else if (strcmp(token, "diaassetruntime")    == 0) mask |= 1u << 3;
						else if (strcmp(token, "diaanimation")       == 0) mask |= 1u << 4;
						else if (strcmp(token, "all")                == 0) mask  = ~0u;
						// unknown tokens silently ignored

						cursor += tokenLen;
						if (comma != nullptr)
							++cursor; // skip comma
					}

					config.profileEnabled      = true;
					config.profileCategoryMask = mask;
					continue;
				}

				// Unknown --log-* flags are silently ignored
			}
		}

	} // namespace Observation
} // namespace Dia
