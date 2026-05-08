#pragma once

namespace Dia
{
	namespace Logger
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
}
