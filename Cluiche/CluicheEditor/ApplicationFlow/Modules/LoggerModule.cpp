#include "LoggerModule.h"
#include "EditorViewModule.h"

#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogLevel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>

#include <string.h>
#include <fstream>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC LoggerModule::kTypeId("EditorLoggerModule");

		LoggerModule::LoggerModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kUpdate)
			, mViewModule(nullptr)
			, mConsoleSinkBridgeConnected(false)
		{
		}

		LoggerModule::~LoggerModule()
		{
		}

		void LoggerModule::ApplyConfig(const char* configPath)
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

			Dia::Logger::ISink* sinks[] = { &mDebugOutputSink, &mConsoleSink };
			const unsigned int sinkCount = 2;

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
						if (strcmp(sinks[s]->GetName(), sinkName) == 0)
						{
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
			}
			else
			{
				for (unsigned int i = 0; i < sinkCount; ++i)
					sinks[i]->SetLevelThreshold(defaultLevel);
			}
		}

		void LoggerModule::DisconnectConsoleSinkBridge()
		{
			mConsoleSink.SetBridge(nullptr);
			mConsoleSinkBridgeConnected = false;
			mViewModule = nullptr;
		}

		void LoggerModule::ObserverNotification(const Dia::Core::ObserverSubject* /*subject*/, int /*message*/)
		{
			DisconnectConsoleSinkBridge();
		}

		Dia::Application::StateObject::OpertionResponse LoggerModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.RegisterThreadBuffer();
			logger.RegisterSink(&mDebugOutputSink);
			logger.RegisterSink(&mConsoleSink);

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void LoggerModule::DoUpdate()
		{
			Dia::Logger::Logger::Instance().FlushBuffers();

			if (!mConsoleSinkBridgeConnected && mViewModule != nullptr && mViewModule->HasStarted())
			{
				Dia::Editor::WebUIBridge* bridge =
					mViewModule->GetView().GetWebUIBridge();
				if (bridge != nullptr)
				{
					mConsoleSink.SetBridge(bridge);
					mConsoleSinkBridgeConnected = true;
					mViewModule->AttachToObserver(this);

					bridge->RegisterEventHandler(Dia::Core::StringCRC("console_ready"),
						[this](const Json::Value& /*data*/)
						{
							mConsoleSink.NotifyConsoleReady();
						});
				}
			}
		}

		void LoggerModule::DoStop()
		{
			DisconnectConsoleSinkBridge();

			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.UnregisterSink(&mDebugOutputSink);
			logger.UnregisterSink(&mConsoleSink);
			logger.UnregisterThreadBuffer();
		}
	}
}
