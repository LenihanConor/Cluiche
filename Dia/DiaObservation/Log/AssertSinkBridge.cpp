#include "DiaObservation/Log/AssertSinkBridge.h"
#include "DiaObservation/Log/Logger.h"
#include "DiaObservation/Log/LogEntry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

namespace Dia
{
	namespace Observation { namespace Log
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
} // namespace Observation
} // namespace Dia
