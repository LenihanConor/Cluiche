#pragma once

#include <DiaCore/CRC/StringCRC.h>

#include "LevelFlow\Phases\MainLoadPhase.h"

namespace Cluiche
{
	namespace DummyLevel
	{
		/*class ApplicationBundle
		{
		public:

		};*/
	
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: Level
		////////////////////////////////////////////////////////////////////////////////
		class Level//: ApplicationBundle
		{
		public:
			static const Dia::Core::StringCRC kLevelUniqueId; //("dummy_level"); //("dummy_project"; // This is registered with the level manager and is used by UI/flow to determine what to boot.
			static const Dia::Core::StringCRC kEntryPhase;// (Cluiche::DummyProject::MainLoadPhase::kUniqueId); // This is the id of the phase we should use to enter this mode 	

			Level(Dia::Application::ProcessingUnit* associatedProcessingUnit)
				: mAssociatedProcessingUnit(associatedProcessingUnit)
				, mMainLoadPhase(associatedProcessingUnit)
			{}

			void Initialize()
			{
				mAssociatedProcessingUnit
			}

		private:

			Dia::Application::ProcessingUnit* mAssociatedProcessingUnit;
			MainLoadPhase mMainLoadPhase;
		};		
	}
}
