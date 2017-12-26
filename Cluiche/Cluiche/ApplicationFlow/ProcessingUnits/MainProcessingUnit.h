#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "ApplicationFlow/Modules/LevelRegistryModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "ApplicationFlow/Phases/MainBootPhase.h"
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include "ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h"
#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

namespace Cluiche
{
	/// Main game booting processing unit. this should run on the main thread
	class MainProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MainProcessingUnit();

	private:
		virtual void PostPhaseStart(const IStartData* startData)override final;
		virtual void PrePhaseStop()override final;
		virtual bool FlaggedToStopUpdating()const override final;

		//Processing Units
		std::thread* mRenderThread;
		Cluiche::RenderProcessingUnit mRenderingPU;

		std::thread* mSimThread;
		Cluiche::SimProcessingUnit mSimPU;

		//Phases
		Cluiche::MainBootPhase mBootPhase;
		Cluiche::MainBootStrapPhase mBootStrapPhase;

		//Modules
		Cluiche::Kernel::MainKernelModule mKernelModule;
		Cluiche::LevelRegistryModule mLevelRegistryModule;
		Cluiche::MainUIModule mUI;
	};
}
