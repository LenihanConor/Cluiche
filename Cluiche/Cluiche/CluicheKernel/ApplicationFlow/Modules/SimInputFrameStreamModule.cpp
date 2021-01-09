#include "ApplicationFlow/Modules/SimInputFrameStreamModule.h"

namespace Cluiche
{
	namespace Sim
	{
		const Dia::Core::StringCRC InputFrameStreamModule::kUniqueId("Sim::InputFrameStreamModule");

		InputFrameStreamModule::InputFrameStreamModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle)
			, mInputToSimFrameStream(nullptr)
		{}

		void InputFrameStreamModule::Initialize(InputFrameStream* stream)
		{
			mInputToSimFrameStream = stream;
		}

		const InputFrameStreamModule::InputFrameStream* InputFrameStreamModule::GetStream()const
		{
			return mInputToSimFrameStream;
		}

		InputFrameStreamModule::InputFrameStream* InputFrameStreamModule::GetStream()
		{
			return mInputToSimFrameStream;
		}
	}
}