#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaInput/EventData.h>
#include <DiaCore/Frame/FrameStream.h>

namespace Cluiche
{
	namespace Sim
	{
		////////////////////////////////////////////////////
		//
		// SimInputFrameStreammModule: Sim access to the input stream
		//
		////////////////////////////////////////////////////
		class InputFrameStreamModule : public Dia::Application::Module
		{
		public:
			typedef Dia::Core::FrameStream<Dia::Input::EventData> InputFrameStream;

			static const Dia::Core::StringCRC kTypeId;

			InputFrameStreamModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId = kTypeId);

			void Initialize(InputFrameStream* stream);

			const InputFrameStream* GetStream()const;
			InputFrameStream* GetStream();

		private:
			InputFrameStream* mInputToSimFrameStream;
		};
	}
}
