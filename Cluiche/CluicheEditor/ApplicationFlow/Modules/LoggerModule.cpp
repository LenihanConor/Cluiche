#include "LoggerModule.h"

#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>

#include <string.h>

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
