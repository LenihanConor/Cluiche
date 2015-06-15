#include "Source/ApplicationModel/Modules/ApplicationTimeModule.h"

const Dia::Core::StringCRC ApplicationTimeModule::kUniqueId("ApplicationTimeModule");

ApplicationTimeModule::ApplicationTimeModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
	: Dia::Application::Module(associatedProcessingUnit, kUniqueId, RunningEnum::kIdle)
{}

ApplicationTimeModule::~ApplicationTimeModule()
{}

Dia::Application::StateObject::OpertionResponse ApplicationTimeModule::DoStart() 
{ 
	mTimer.Start();
	return StateObject::OpertionResponse::kImmediate; 
}
Dia::Core::TimeRelative	ApplicationTimeModule::IsRunningFor()const
{
	return mTimer.IsRunningFor();
}
const Dia::Core::TimeAbsolute& ApplicationTimeModule::StartTime()const
{
	return mTimer.StartTime();
}