#include "LoggerModule.h"

#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogLevel.h>
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
			, mOwnedSinkCount(0)
		{
			memset(mOwnedSinks, 0, sizeof(mOwnedSinks));
		}

		LoggerModule::~LoggerModule()
		{
		}

		void LoggerModule::AddSink(Dia::Logger::ISink* sink)
		{
			if (sink == nullptr || mOwnedSinkCount >= kMaxSinks)
				return;

			mOwnedSinks[mOwnedSinkCount++] = sink;
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

			for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
			{
				if (mOwnedSinks[i] != nullptr)
					mOwnedSinks[i]->SetLevelThreshold(defaultLevel);
			}

			if (root.isMember("sinks") && root["sinks"].isArray())
			{
				const Json::Value& sinks = root["sinks"];
				for (Json::ArrayIndex i = 0; i < sinks.size(); ++i)
				{
					const Json::Value& sinkConfig = sinks[i];
					if (!sinkConfig.isMember("name") || !sinkConfig["name"].isString())
						continue;

					const char* sinkName = sinkConfig["name"].asCString();
					for (unsigned int s = 0; s < mOwnedSinkCount; ++s)
					{
						if (mOwnedSinks[s] != nullptr && strcmp(mOwnedSinks[s]->GetName(), sinkName) == 0)
						{
							if (sinkConfig.isMember("level_threshold") && sinkConfig["level_threshold"].isString())
								mOwnedSinks[s]->SetLevelThreshold(Dia::Logger::LogLevelFromString(sinkConfig["level_threshold"].asCString(), defaultLevel));

							if (sinkConfig.isMember("channels") && sinkConfig["channels"].isArray())
							{
								mOwnedSinks[s]->ClearChannelFilter();
								const Json::Value& channels = sinkConfig["channels"];
								for (Json::ArrayIndex c = 0; c < channels.size(); ++c)
								{
									if (channels[c].isString())
										mOwnedSinks[s]->SetChannelFilter(Dia::Core::StringCRC(channels[c].asCString()), true);
								}
							}
							break;
						}
					}
				}
			}
		}

		Dia::Application::StateObject::OpertionResponse LoggerModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.RegisterThreadBuffer();

			for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
				logger.RegisterSink(mOwnedSinks[i]);

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void LoggerModule::DoUpdate()
		{
			Dia::Logger::Logger::Instance().FlushBuffers();
		}

		void LoggerModule::DoStop()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();

			for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
				logger.UnregisterSink(mOwnedSinks[i]);

			logger.UnregisterThreadBuffer();

			for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
			{
				delete mOwnedSinks[i];
				mOwnedSinks[i] = nullptr;
			}
			mOwnedSinkCount = 0;
		}
	}
}
