#include "ApplicationFlow/Modules/SimInputFrameStreamModule.h"

namespace Cluiche
{
	namespace Sim
	{
		const Dia::Core::StringCRC InputFrameStreamModule::kTypeId("Sim::InputFrameStreamModule");

		InputFrameStreamModule::InputFrameStreamModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
			: Dia::Application::Module(associatedProcessingUnit, instanceId, Dia::Application::Module::RunningEnum::kIdle)
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

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _InputFrameStreamModule = Cluiche::Sim::InputFrameStreamModule; }
DIA_REGISTER_MODULE(_InputFrameStreamModule) {
	return new Cluiche::Sim::InputFrameStreamModule(pu, instanceId);
}