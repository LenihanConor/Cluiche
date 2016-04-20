#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "ApplicationFlow/Phases/RenderRunningPhase.h"

#include <DiaCore/Frame/FrameStream.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Dia { namespace Graphics { class ICanvas; } }

namespace Cluiche
{
	/// Main game booting processing unit. this should run on the main thread
	class RenderProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		class StartData: public Dia::Application::ProcessingUnit::IStartData
		{
		public:
			const bool* mRunning;
			Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
			Dia::Graphics::ICanvas* mCanvas;
		};

		static const Dia::Core::StringCRC kUniqueId;

		RenderProcessingUnit();

	private:
		virtual void PostPhaseStart(const IStartData* startData) override final;
		virtual void PostPhaseUpdate() override final;
		virtual bool FlaggedToStopUpdating()const override final;
		
		// Shared resources
		const bool* mRunning;
		Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
		Dia::Graphics::ICanvas* mpCanvas;

		//Phases
		Cluiche::RenderRunningPhase mRunningPhase;
	};
}
