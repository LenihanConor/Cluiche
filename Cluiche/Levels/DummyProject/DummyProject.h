#pragma once

#include "DiaCore/CRC/StringCRC.h"

#include "ApplicationFlow/Phases/MainLoadPhase.h"

namespace Cluiche
{
	namespace DummyProject
	{
		static const Dia::Core::StringCRC kLevelUniqueId("dummy_project"); //("dummy_project"; // This is registered with the level manager and is used by UI/flow to determine what to boot.
		static const Dia::Core::StringCRC kEntryPhase = MainLoadPhase::kUniqueId; // This is the id of the phase we should use to enter this mode 	
	}
}
