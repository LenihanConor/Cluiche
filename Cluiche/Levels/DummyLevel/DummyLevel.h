#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <CluicheKernel/LevelRegistry.h>

#include "LevelFlow\Phases\MainLoadPhase.h"

namespace Cluiche
{
	namespace DummyLevel
	{
		class Level : public Cluiche::Kernel::ILevel
		{
		public:
			static const Dia::Core::StringCRC kLevelUniqueId; // This is registered with the level manager and is used by UI/flow to determine what to boot.
			
			Level(Dia::Application::Phase* currentPhase, Dia::Application::ProcessingUnit* mainPU, Dia::Application::ProcessingUnit* simPU, Dia::Application::ProcessingUnit* renderPU);

			virtual const Dia::Core::StringCRC& GetUniqueId()const final{ return kLevelUniqueId; }
			virtual const Dia::Core::StringCRC& GetEntryPhaseUniqueId()const final {return mEntryPhaseUniqueId;}

		private:
			MainLoadPhase mMainLoadPhase;
			Dia::Core::StringCRC mEntryPhaseUniqueId; // This is the id of the phase we should use to enter this mode 	

		};		
	}
}
