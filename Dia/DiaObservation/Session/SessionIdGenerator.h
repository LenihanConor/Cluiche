#pragma once

namespace Dia
{
	namespace Observation { namespace Internal
	{
		// Writes "YYYYMMDD-HHMMSS-XXXXXXXX\0" into out[32].
		// Uses UTC time + mt19937 seeded from steady_clock ^ process id.
		void GenerateSessionId(char (&out)[32]);
	}
} // namespace Observation
} // namespace Dia
