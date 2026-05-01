#pragma once

#include <DiaApplication/ApplicationModule.h>

namespace Dia { namespace Logger { class ISink; } }

namespace Cluiche
{
	class LoggerModule : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kTypeId;

		LoggerModule(Dia::Application::ProcessingUnit* pu);
		~LoggerModule();

		void AddSink(Dia::Logger::ISink* sink);

	protected:
		Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
		void DoUpdate() override;
		void DoStop() override;

	private:
		static const unsigned int kMaxSinks = 8;
		Dia::Logger::ISink* mOwnedSinks[kMaxSinks];
		unsigned int mOwnedSinkCount;
	};
}
