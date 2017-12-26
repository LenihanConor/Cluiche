#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "ApplicationFlow/Phases/SimBootStrapPhase.h"
#include "ApplicationFlow/Phases/SimBootPhase.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h"

#include <DiaCore/Frame/FrameStream.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Dia { namespace Graphics { class ICanvas; } }
namespace Cluiche { class MainUIModule; }

namespace Cluiche
{
	class SimProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		class StartData : public Dia::Application::ProcessingUnit::IStartData
		{
		public:
			const bool* mRunning;
			Cluiche::MainUIModule* mMainUIModule;
			Dia::Core::FrameStream<Dia::Input::EventData>* mInputToSimFrameStream;
			Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
		};

		static const Dia::Core::StringCRC kUniqueId;

		SimProcessingUnit();

	private:
		virtual void PostPhaseStart(const IStartData* startData) override final;
		virtual void PrePhaseUpdate() override final;
		virtual void PostPhaseUpdate() override final;
		virtual bool FlaggedToStopUpdating()const override final;


		Dia::Graphics::FrameData mRenderFrameBuffer;
		
		// Shared resources
		const bool* mRunning;
		Dia::Core::FrameStream<Dia::Graphics::FrameData>* mSimToRenderFrameStream;

		//Phases
		Cluiche::SimBootPhase mBootPhase;
		Cluiche::SimBootStrapPhase mBootStrapPhase;

		//Modules
		Cluiche::SimTimeServerModule mSimTimeServerModule;
		Cluiche::SimUIProxyModule mSimUIProxyModule;
		Cluiche::SimInputFrameStreamModule mSimInputFrameStreamModule;
	};
}
