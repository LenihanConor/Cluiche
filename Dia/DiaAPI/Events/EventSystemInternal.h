////////////////////////////////////////////////////////////////////////////////
// Filename: EventSystemInternal.h
// Description: Internal event firing functions - not part of public DiaAPI surface
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace API
	{
		struct CommandArgs;

		namespace Internal
		{
			void FireCommandRegistered(const Dia::Core::StringCRC& name, const char* description);
			void FireCommandExecuting(const Dia::Core::StringCRC& name, const CommandArgs* args);
			void FireCommandExecuted(const Dia::Core::StringCRC& name, int exitCode, float duration);
			void FireCommandError(const Dia::Core::StringCRC& name, const char* errorMessage, int exitCode);
			void FireHelpRequested(const Dia::Core::StringCRC& commandName, bool isGlobalHelp);
		}
	}
}
