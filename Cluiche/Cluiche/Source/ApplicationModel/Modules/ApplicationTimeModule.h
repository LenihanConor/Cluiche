#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore\CRC\StringCRC.h>
#include <DiaCore/Timer/TimerSystem.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Class name: ApplicationTimeModule, Timer that never stops and calulates how long the app has been running
////////////////////////////////////////////////////////////////////////////////
class ApplicationTimeModule: public Dia::Application::Module
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	ApplicationTimeModule(Dia::Application::ProcessingUnit* associatedProcessingUnit);

	virtual ~ApplicationTimeModule();

	virtual StateObject::OpertionResponse DoStart() override;

	Dia::Core::TimeRelative	IsRunningFor()const;
	const Dia::Core::TimeAbsolute& StartTime()const;

private:
	Dia::Core::TimerSystem mTimer; // Timer on how long the application has been up
};
