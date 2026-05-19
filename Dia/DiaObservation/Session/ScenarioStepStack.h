#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Observation
	{
		class ScenarioStepStack
		{
		public:
			static constexpr unsigned int kMaxDepth = 8;

			static void Push(const Dia::Core::StringCRC& step);
			static void Pop();
			static Dia::Core::StringCRC Current();
		};
	}
} // namespace Dia
