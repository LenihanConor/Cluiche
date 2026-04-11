#ifndef DIA_LOG_CONFIG_H
#define DIA_LOG_CONFIG_H

#include "DiaCore/Core/Logging/LogLevel.h"
#include "DiaCore/Core/Logging/LogNamespace.h"
#include "DiaCore/Core/Logging/LogChannel.h"
#include "DiaCore/Threading/Mutex.h"
#include "DiaCore/Json/external/json/json.h"
#include <unordered_map>
#include <fstream>

namespace Dia
{
	namespace Core
	{
		namespace Logging
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Configuration Entry
			//
			// Configuration for a specific namespace or individual log point.
			// Supports:
			//   - Minimum log level
			//   - Channel routing (where to output)
			//   - Suppression flag
			//---------------------------------------------------------------------------------------------------------------------------------

			struct LogConfigEntry
			{
				LogLevel minLevel;
				LogChannelMask channels;
				bool suppressed;

				LogConfigEntry()
					: minLevel(LogLevel::Info)
					, channels(LogChannel_Console)
					, suppressed(false)
				{}

				LogConfigEntry(LogLevel level, LogChannelMask ch, bool sup = false)
					: minLevel(level)
					, channels(ch)
					, suppressed(sup)
				{}
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Configuration
			//
			// Manages logging configuration from JSON files and runtime overrides.
			//
			// USAGE:
			//   LogConfig config;
			//   config.LoadFromFile("logging_config.json");
			//
			//   // Runtime override
			//   config.SetNamespaceConfig("Physics", LogLevel::Debug, LogChannel_Console | LogChannel_File);
			//   config.SuppressNamespace("Audio", true);
			//
			// JSON FORMAT:
			//   {
			//     "default": {
			//       "level": "INFO",
			//       "channels": "Console|File",
			//       "suppressed": false
			//     },
			//     "namespaces": {
			//       "Physics": {
			//         "level": "DEBUG",
			//         "channels": "Console|File|Server"
			//       },
			//       "Audio": {
			//         "suppressed": true
			//       }
			//     },
			//     "individual": {
			//       "Physics.cpp:142": {
			//         "suppressed": true
			//       }
			//     }
			//   }
			//
			// FEATURES:
			//   - Load configuration from JSON file
			//   - Runtime overrides (for live service/debugging)
			//   - Per-namespace configuration
			//   - Per-file:line suppression
			//   - Hierarchical inheritance (namespace → parent → default)
			//---------------------------------------------------------------------------------------------------------------------------------

			class LogConfig
			{
			public:
				LogConfig()
					: mDefaultConfig(LogLevel::Info, LogChannel_Console, false)
				{}

				~LogConfig() {}

				//---------------------------------------------------------
				// Configuration Loading
				//---------------------------------------------------------

				// Load configuration from JSON file
				bool LoadFromFile(const char* filePath)
				{
					std::ifstream file(filePath);
					if (!file.is_open())
					{
						return false;
					}

					Json::Value root;
					Json::Reader reader;
					if (!reader.parse(file, root))
					{
						return false;
					}

					ScopedLock<Mutex> lock(mMutex);

					// Load default config
					if (root.isMember("default"))
					{
						mDefaultConfig = ParseConfigEntry(root["default"]);
					}

					// Load namespace configs
					if (root.isMember("namespaces"))
					{
						const Json::Value& namespaces = root["namespaces"];
						for (const auto& key : namespaces.getMemberNames())
						{
							mNamespaceConfigs[LogNamespace(key.c_str())] = ParseConfigEntry(namespaces[key]);
						}
					}

					// Load individual suppression (file:line)
					if (root.isMember("individual"))
					{
						const Json::Value& individual = root["individual"];
						for (const auto& key : individual.getMemberNames())
						{
							mIndividualConfigs[key] = ParseConfigEntry(individual[key]);
						}
					}

					return true;
				}

				// Save current configuration to JSON file
				bool SaveToFile(const char* filePath) const
				{
					ScopedLock<Mutex> lock(mMutex);

					Json::Value root;

					// Save default config
					root["default"] = SerializeConfigEntry(mDefaultConfig);

					// Save namespace configs
					Json::Value namespaces;
					for (const auto& pair : mNamespaceConfigs)
					{
						namespaces[pair.first.GetName()] = SerializeConfigEntry(pair.second);
					}
					root["namespaces"] = namespaces;

					// Save individual configs
					Json::Value individual;
					for (const auto& pair : mIndividualConfigs)
					{
						individual[pair.first] = SerializeConfigEntry(pair.second);
					}
					root["individual"] = individual;

					// Write to file
					std::ofstream file(filePath);
					if (!file.is_open())
					{
						return false;
					}

					Json::StyledWriter writer;
					file << writer.write(root);
					return true;
				}

				//---------------------------------------------------------
				// Runtime Configuration
				//---------------------------------------------------------

				// Set default configuration
				void SetDefaultConfig(LogLevel minLevel, LogChannelMask channels)
				{
					ScopedLock<Mutex> lock(mMutex);
					mDefaultConfig.minLevel = minLevel;
					mDefaultConfig.channels = channels;
				}

				// Set configuration for namespace
				void SetNamespaceConfig(const LogNamespace& ns, LogLevel minLevel, LogChannelMask channels)
				{
					ScopedLock<Mutex> lock(mMutex);
					mNamespaceConfigs[ns] = LogConfigEntry(minLevel, channels, false);
				}

				// Suppress namespace
				void SuppressNamespace(const LogNamespace& ns, bool suppressed)
				{
					ScopedLock<Mutex> lock(mMutex);
					mNamespaceConfigs[ns].suppressed = suppressed;
				}

				// Suppress individual log point (file:line)
				void SuppressIndividual(const char* fileAndLine, bool suppressed)
				{
					ScopedLock<Mutex> lock(mMutex);
					mIndividualConfigs[fileAndLine].suppressed = suppressed;
				}

				//---------------------------------------------------------
				// Configuration Query
				//---------------------------------------------------------

				// Get configuration for namespace
				LogConfigEntry GetConfig(const LogNamespace& ns) const
				{
					ScopedLock<Mutex> lock(mMutex);

					// Check exact namespace match
					auto it = mNamespaceConfigs.find(ns);
					if (it != mNamespaceConfigs.end())
					{
						return it->second;
					}

					// Check parent namespaces (hierarchical)
					// e.g., "Physics.Collision" → check "Physics"
					for (const auto& pair : mNamespaceConfigs)
					{
						if (ns.StartsWith(pair.first))
						{
							return pair.second;
						}
					}

					// Fall back to default
					return mDefaultConfig;
				}

				// Check if individual log point is suppressed
				bool IsIndividualSuppressed(const char* file, int line) const
				{
					ScopedLock<Mutex> lock(mMutex);

					// Build "file:line" key
					char key[512];
					snprintf(key, sizeof(key), "%s:%d", file, line);

					auto it = mIndividualConfigs.find(key);
					if (it != mIndividualConfigs.end())
					{
						return it->second.suppressed;
					}

					return false;
				}

				// Get default configuration
				LogConfigEntry GetDefaultConfig() const
				{
					ScopedLock<Mutex> lock(mMutex);
					return mDefaultConfig;
				}

				//---------------------------------------------------------
				// Clear Configuration
				//---------------------------------------------------------

				void Clear()
				{
					ScopedLock<Mutex> lock(mMutex);
					mNamespaceConfigs.clear();
					mIndividualConfigs.clear();
					mDefaultConfig = LogConfigEntry(LogLevel::Info, LogChannel_Console, false);
				}

			private:
				//---------------------------------------------------------
				// JSON Parsing
				//---------------------------------------------------------

				LogConfigEntry ParseConfigEntry(const Json::Value& json) const
				{
					LogConfigEntry entry;

					if (json.isMember("level"))
					{
						entry.minLevel = StringToLogLevel(json["level"].asCString());
					}

					if (json.isMember("channels"))
					{
						entry.channels = StringToLogChannel(json["channels"].asCString());
					}

					if (json.isMember("suppressed"))
					{
						entry.suppressed = json["suppressed"].asBool();
					}

					return entry;
				}

				Json::Value SerializeConfigEntry(const LogConfigEntry& entry) const
				{
					Json::Value json;
					json["level"] = LogLevelToString(entry.minLevel);

					char channelStr[256];
					LogChannelToString(entry.channels, channelStr, sizeof(channelStr));
					json["channels"] = channelStr;

					json["suppressed"] = entry.suppressed;
					return json;
				}

				//---------------------------------------------------------
				// Data
				//---------------------------------------------------------

				LogConfigEntry mDefaultConfig;
				std::unordered_map<StringCRC, LogConfigEntry> mNamespaceConfigs;
				std::unordered_map<std::string, LogConfigEntry> mIndividualConfigs; // "file:line" → config

				mutable Mutex mMutex;
			};
		}
	}
}

#endif // DIA_LOG_CONFIG_H
