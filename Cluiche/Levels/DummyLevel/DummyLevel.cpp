////////////////////////////////////////////////////////////////////////////////
// Filename: DummyProject
////////////////////////////////////////////////////////////////////////////////
#include "DummyLevel/DummyLevel.h"

namespace Cluiche
{
	namespace DummyLevel
	{
		const Dia::Core::StringCRC Level::kLevelUniqueId("dummy_level"); //("dummy_project"; // This is registered with the level manager and is used by UI/flow to determine what to boot.
		const Dia::Core::StringCRC Level::kEntryPhase(Cluiche::DummyLevel::MainLoadPhase::kUniqueId);
	}
}