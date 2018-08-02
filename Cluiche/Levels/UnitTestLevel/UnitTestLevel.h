#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <CluicheKernel/LevelRegistry.h>

#include "LevelFlow\Phases\MainLoadPhase.h"
#include "LevelFlow\Phases\MainFEPhase.h"

namespace Cluiche
{
	namespace UnitTestLevel
	{
		class Level : public Cluiche::Kernel::ILevel
		{
		public:
			static const Dia::Core::StringCRC kLevelUniqueId; // This is registered with the level manager and is used by UI/flow to determine what to boot.
			
			Level(Dia::Application::Phase* currentPhase, Dia::Application::ProcessingUnit* mainPU, Dia::Application::ProcessingUnit* simPU, Dia::Application::ProcessingUnit* renderPU);
			~Level();

			virtual const Dia::Core::StringCRC& GetUniqueId()const final{ return kLevelUniqueId; }
			virtual const Dia::Core::StringCRC& GetEntryPhaseUniqueId()const final {return mEntryPhaseUniqueId;}
			virtual const Dia::Core::StringCRC& GetExitPhaseUniqueId()const final { return mExitPhaseUniqueId; }

		private:
			MainLoadPhase mMainLoadPhase;
			MainFEPhase mMainFEPhase;
			Dia::Core::StringCRC mEntryPhaseUniqueId; // This is the id of the phase we should use to enter this mode
			Dia::Core::StringCRC mExitPhaseUniqueId; // This is the id of the phase we should return to after exiting mode
		};		
	}
}
