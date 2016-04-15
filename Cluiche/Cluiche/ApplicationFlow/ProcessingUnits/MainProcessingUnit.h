#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "ApplicationFlow/Modules/MainKernelModule.h"
#include "ApplicationFlow/Phases/MainBootPhase.h"
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

namespace Cluiche
{
	/// Main game booting processing unit. this should run on the main thread
	class MainProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MainProcessingUnit();

	private:
		virtual bool FlaggedToStopUpdating()const;

		//Phases
		Cluiche::MainBootPhase mBootPhase;
		Cluiche::MainBootStrapPhase mBootStrapPhase;

		//Modules
		Cluiche::MainKernelModule mKernelModule;
	};
}
