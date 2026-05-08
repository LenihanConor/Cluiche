#include "LoggerModule.h"

#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>

namespace Cluiche
{
	const Dia::Core::StringCRC LoggerModule::kTypeId("LoggerModule");

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

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
#include <DiaLogger/DebugOutputSink.h>
#include <DiaLogger/LogLevel.h>

namespace { using _LoggerModule = Cluiche::LoggerModule; }
DIA_REGISTER_MODULE(_LoggerModule) {
	Cluiche::LoggerModule* mod = new Cluiche::LoggerModule(pu);

	Dia::Logger::LogLevel defaultLevel = Dia::Logger::LogLevel::kInfo;
	if (config.isMember("default_level") && config["default_level"].isString())
		defaultLevel = Dia::Logger::LogLevelFromString(config["default_level"].asCString());

	if (config.isMember("sinks") && config["sinks"].isArray())
	{
		const Json::Value& sinks = config["sinks"];
		for (Json::ArrayIndex i = 0; i < sinks.size(); ++i)
		{
			const Json::Value& sinkConfig = sinks[i];
			if (!sinkConfig.isMember("type") || !sinkConfig["type"].isString())
				continue;

			const char* sinkType = sinkConfig["type"].asCString();
			Dia::Logger::ISink* sink = nullptr;

			if (strcmp(sinkType, "DebugOutput") == 0)
				sink = new Dia::Logger::DebugOutputSink();

			if (sink != nullptr)
			{
				if (sinkConfig.isMember("level_threshold") && sinkConfig["level_threshold"].isString())
					sink->SetLevelThreshold(Dia::Logger::LogLevelFromString(sinkConfig["level_threshold"].asCString(), defaultLevel));
				else
					sink->SetLevelThreshold(defaultLevel);

				if (sinkConfig.isMember("channels") && sinkConfig["channels"].isArray())
				{
					const Json::Value& channels = sinkConfig["channels"];
					for (Json::ArrayIndex c = 0; c < channels.size(); ++c)
					{
						if (channels[c].isString())
							sink->SetChannelFilter(Dia::Core::StringCRC(channels[c].asCString()), true);
					}
				}

				mod->AddSink(sink);
			}
		}
	}
	else
	{
		Dia::Logger::DebugOutputSink* sink = new Dia::Logger::DebugOutputSink();
		sink->SetLevelThreshold(defaultLevel);
		mod->AddSink(sink);
	}

	return mod;
}
