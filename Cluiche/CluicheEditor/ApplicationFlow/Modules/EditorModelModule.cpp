#include "EditorModelModule.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogLevel.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <string.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorModelModule::kTypeId("EditorModelModule");

		namespace
		{
			// Parse the single project path argument from GetCommandLineW() — strip
			// the exe name, whitespace, and surrounding quotes; convert to UTF-8.
			void ParseProjectPathFromCommandLine(char* outUtf8, unsigned int outSize)
			{
				if (outSize == 0) return;
				outUtf8[0] = '\0';

				LPWSTR fullCmd = GetCommandLineW();
				if (fullCmd == nullptr || fullCmd[0] == L'\0') return;

				// Skip the exe argv[0] — either a quoted string or a whitespace-terminated token.
				LPWSTR p = fullCmd;
				if (*p == L'"')
				{
					++p;
					while (*p && *p != L'"') ++p;
					if (*p == L'"') ++p;
				}
				else
				{
					while (*p && *p != L' ' && *p != L'\t') ++p;
				}

				// Skip whitespace between argv[0] and argv[1].
				while (*p == L' ' || *p == L'\t') ++p;
				if (*p == L'\0') return;

				// Strip surrounding quotes from the project path arg.
				wchar_t buf[260] = {0};
				LPWSTR dest = buf;
				LPWSTR end   = buf + (sizeof(buf) / sizeof(buf[0])) - 1;
				if (*p == L'"')
				{
					++p;
					while (*p && *p != L'"' && dest < end) *dest++ = *p++;
				}
				else
				{
					while (*p && dest < end) *dest++ = *p++;
				}
				*dest = L'\0';

				WideCharToMultiByte(CP_UTF8, 0, buf, -1, outUtf8, static_cast<int>(outSize), nullptr, nullptr);
			}

			// Apply editor-logger.json (if present): sets default level + per-sink thresholds/channels.
			void ApplyLoggerConfig(const char* configPath, Dia::Logger::ISink** sinks, unsigned int sinkCount)
			{
				if (configPath == nullptr) return;

				std::ifstream file(configPath);
				if (!file.is_open()) return;

				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(file, root)) return;

				Dia::Logger::LogLevel defaultLevel = Dia::Logger::LogLevel::kInfo;
				if (root.isMember("default_level") && root["default_level"].isString())
					defaultLevel = Dia::Logger::LogLevelFromString(root["default_level"].asCString());

				if (root.isMember("sinks") && root["sinks"].isArray())
				{
					const Json::Value& sinksConfig = root["sinks"];
					for (Json::ArrayIndex i = 0; i < sinksConfig.size(); ++i)
					{
						const Json::Value& sinkConfig = sinksConfig[i];
						if (!sinkConfig.isMember("name") || !sinkConfig["name"].isString())
							continue;

						const char* sinkName = sinkConfig["name"].asCString();
						for (unsigned int s = 0; s < sinkCount; ++s)
						{
							if (strcmp(sinks[s]->GetName(), sinkName) != 0) continue;

							if (sinkConfig.isMember("level_threshold") && sinkConfig["level_threshold"].isString())
								sinks[s]->SetLevelThreshold(Dia::Logger::LogLevelFromString(sinkConfig["level_threshold"].asCString(), defaultLevel));
							else
								sinks[s]->SetLevelThreshold(defaultLevel);

							if (sinkConfig.isMember("channels") && sinkConfig["channels"].isArray())
							{
								sinks[s]->ClearChannelFilter();
								const Json::Value& channels = sinkConfig["channels"];
								for (Json::ArrayIndex c = 0; c < channels.size(); ++c)
								{
									if (channels[c].isString())
										sinks[s]->SetChannelFilter(Dia::Core::StringCRC(channels[c].asCString()), true);
								}
							}
							break;
						}
					}
				}
				else
				{
					for (unsigned int i = 0; i < sinkCount; ++i)
						sinks[i]->SetLevelThreshold(defaultLevel);
				}
			}
		} // anonymous namespace

		EditorModelModule::EditorModelModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
		{
			mProjectPath[0] = '\0';
		}

		void EditorModelModule::SetProjectPath(const char* path)
		{
			if (path == nullptr)
			{
				mProjectPath[0] = '\0';
				return;
			}
			strncpy_s(mProjectPath, kMaxProjectPathLength, path, _TRUNCATE);
		}

		const char* EditorModelModule::GetProjectPath() const
		{
			return mProjectPath;
		}

		Dia::ApplicationFlow::StartResult EditorModelModule::DoStart()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.RegisterThreadBuffer();
			logger.RegisterSink(&mDebugOutputSink);

			Dia::Logger::ISink* sinks[] = { &mDebugOutputSink };
			ApplyLoggerConfig("assets/configs/editor-logger.json", sinks, 1);

			ParseProjectPathFromCommandLine(mProjectPath, kMaxProjectPathLength);

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void EditorModelModule::DoUpdate(float /*deltaTime*/)
		{
			Dia::Logger::Logger::Instance().FlushBuffers();
		}

		Dia::ApplicationFlow::StopResult EditorModelModule::DoStop()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.UnregisterSink(&mDebugOutputSink);
			logger.UnregisterThreadBuffer();
			mModel.Reset();
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using EditorModelModule_ = Cluiche::Editor::EditorModelModule; }
DIA_MODULE(EditorModelModule_);
