#pragma once

#include <cstdio>
#include <process.h>

namespace Dia
{
	namespace Observation { namespace Testing
	{
		// A test output file name made unique per process ("base-<pid>.ext") so that
		// sharded test processes sharing a working directory never collide on the file.
		// Converts implicitly to const char*.
		class UniqueTestPath
		{
		public:
			explicit UniqueTestPath(const char* name)
			{
				mPath[0] = '\0';
				const char* dot = nullptr;
				for (const char* c = name; *c; ++c)
					if (*c == '.') dot = c;
				const int stem = dot ? static_cast<int>(dot - name) : -1;
				if (stem >= 0)
					snprintf(mPath, sizeof(mPath), "%.*s-%d%s", stem, name, _getpid(), dot);
				else
					snprintf(mPath, sizeof(mPath), "%s-%d", name, _getpid());
			}

			operator const char*() const { return mPath; }
			const char* c_str() const { return mPath; }

		private:
			char mPath[256];
		};
	}
} // namespace Observation
} // namespace Dia
