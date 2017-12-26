#include "ApplicationFlow/Modules/SimInputFrameStreamModule.h"

namespace Cluiche
{
	const Dia::Core::StringCRC SimInputFrameStreamModule::kUniqueId("SimInputFrameStreamModule");

	SimInputFrameStreamModule::SimInputFrameStreamModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle)
		, mInputToSimFrameStream(nullptr)
	{}

	void SimInputFrameStreamModule::Initialize(InputFrameStream* stream)
	{
		mInputToSimFrameStream = stream;
	}

	const SimInputFrameStreamModule::InputFrameStream* SimInputFrameStreamModule::GetStream()const
	{
		return mInputToSimFrameStream;
	}

	SimInputFrameStreamModule::InputFrameStream* SimInputFrameStreamModule::GetStream()
	{
		return mInputToSimFrameStream;
	}
}