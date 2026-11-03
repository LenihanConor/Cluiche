#pragma once

// Test helper (Testing/ convention, no GoogleTest): builds SimHelpStored values for tests that register sim-hosted
// names directly on a DebugServer.

#include <DiaDebugServer/SimHelpInfo.h>

#include <cstring>

namespace Dia::DebugServer::Testing
{
	// A valid stored help (~7 KB) with the given description, returned by value.
	inline SimHelpStored MakeSimHelp(const char* description = "test help")
	{
		SimHelpStored h;
		std::memset(&h, 0, sizeof(h));
		CopyHelp(SimHelpInfo{ description }, h);
		return h;
	}

	// Shared default help. Do not mutate.
	inline const SimHelpStored& DefaultSimHelp()
	{
		static const SimHelpStored sHelp = MakeSimHelp();
		return sHelp;
	}
}
