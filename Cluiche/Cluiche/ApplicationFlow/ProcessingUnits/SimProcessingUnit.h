#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "ApplicationFlow/Phases/SimBootStrapPhase.h"
#include "ApplicationFlow/Phases/SimBootPhase.h"

#include <DiaCore/Frame/FrameStream.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaInput/EventData.h>
#include <DiaCore/Time/TimeServer.h>

namespace Dia { namespace Graphics { class ICanvas; } }
namespace Dia { namespace UI { class IUISystem; } }

namespace Cluiche
{
	class SimProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		class StartData : public Dia::Application::ProcessingUnit::IStartData
		{
		public:
			const bool* mRunning;
			Dia::UI::IUISystem* mUISystem;
			Dia::Core::FrameStream<Dia::Input::EventData>* mInputToSimFrameStream;
			Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
		};

		static const Dia::Core::StringCRC kUniqueId;

		SimProcessingUnit();

	private:
		virtual void PostPhaseStart(const IStartData* startData) override final;
		virtual void PostPhaseUpdate() override final;
		virtual bool FlaggedToStopUpdating()const override final;

		Dia::Core::TimeServer mTimeServer;

		// Shared resources
		const bool* mRunning;
		Dia::UI::IUISystem* mUISystem;
		Dia::Core::FrameStream<Dia::Input::EventData>* mInputToSimFrameStream;
		Dia::Core::FrameStream<Dia::Graphics::FrameData>* mSimToRenderFrameStream;

		//Phases
		Cluiche::SimBootPhase mBootPhase;
		Cluiche::SimBootStrapPhase mBootStrapPhase;
	};
}
