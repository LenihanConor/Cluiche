#pragma once

namespace Dia
{
	namespace Observation { namespace Log
	{
		class AssertSinkBridge
		{
		public:
			static void Install();
			static void Uninstall();

		private:
			static void OnAssertOutput(const char* formattedMessage);
			static bool sInstalled;
		};
	}
} // namespace Observation
} // namespace Dia
