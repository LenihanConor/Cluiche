#include "DiaLogger/AssertSinkBridge.h"
#include "DiaLogger/Logger.h"
#include "DiaLogger/LogEntry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

namespace Dia
{
	namespace Logger
	{
		bool AssertSinkBridge::sInstalled = false;

		void AssertSinkBridge::Install()
		{
			if (sInstalled)
				return;

			Dia::Core::RegisterAssertOutputCallback(&OnAssertOutput);
			sInstalled = true;
		}

		void AssertSinkBridge::Uninstall()
		{
			if (!sInstalled)
				return;

			Dia::Core::UnregisterAssertOutputCallback(&OnAssertOutput);
			sInstalled = false;
		}

		void AssertSinkBridge::OnAssertOutput(const char* formattedMessage)
		{
			LogEntry entry;
			entry.level = LogLevel::kError;
			entry.channel = Dia::Core::StringCRC("Assert");

			strncpy_s(entry.message, sizeof(entry.message), formattedMessage, _TRUNCATE);

			Logger::Instance().DispatchImmediate(entry);
		}
	}
}
