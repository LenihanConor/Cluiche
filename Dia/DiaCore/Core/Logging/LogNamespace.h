#ifndef DIA_LOG_NAMESPACE_H
#define DIA_LOG_NAMESPACE_H

#include "DiaCore/CRC/StringCRC.h"
#include <cstring>

namespace Dia
{
	namespace Core
	{
		namespace Logging
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Namespace
			//
			// Namespace system for grouping log messages and asserts.
			// "Reads like a string but compares by int" using StringCRC.
			//
			// USAGE:
			//   LogNamespace physics("Physics");
			//   LogNamespace rendering("Rendering");
			//
			//   // Fast integer comparison
			//   if (physics == otherNamespace) { ... }
			//
			// FEATURES:
			//   - Fast comparison (integer CRC, not string compare)
			//   - Readable string representation
			//   - Hierarchical namespaces via dot notation ("Physics.Collision")
			//   - Wildcard matching for configuration
			//
			// EXAMPLES:
			//   "Core"
			//   "Physics"
			//   "Physics.Collision"
			//   "Rendering.Shadows"
			//   "Audio.Mixer"
			//---------------------------------------------------------------------------------------------------------------------------------

			class LogNamespace
			{
			public:
				LogNamespace()
					: mName(nullptr)
					, mId(0)
				{}

				LogNamespace(const char* name)
					: mName(name)
					, mId(StringCRC(name))
				{}

				// Get namespace string (for display)
				const char* GetName() const { return mName ? mName : ""; }

				// Get namespace ID (for fast comparison)
				StringCRC GetId() const { return mId; }

				// Comparison operators (by ID, not string)
				bool operator==(const LogNamespace& other) const { return mId == other.mId; }
				bool operator!=(const LogNamespace& other) const { return mId != other.mId; }
				bool operator<(const LogNamespace& other) const { return mId < other.mId; }

				// Check if this namespace starts with a prefix
				// e.g., "Physics.Collision" starts with "Physics"
				bool StartsWith(const LogNamespace& prefix) const
				{
					if (!mName || !prefix.mName) return false;

					size_t prefixLen = strlen(prefix.mName);
					if (strncmp(mName, prefix.mName, prefixLen) == 0)
					{
						// Exact match or followed by '.'
						return mName[prefixLen] == '\0' || mName[prefixLen] == '.';
					}
					return false;
				}

				// Check if namespace matches pattern with wildcards
				// Supports: "*" = any, "Physics.*" = all under Physics
				bool Matches(const char* pattern) const
				{
					if (!pattern || !mName) return false;

					// Simple wildcard matching
					if (strcmp(pattern, "*") == 0) return true;

					size_t patternLen = strlen(pattern);
					if (patternLen > 0 && pattern[patternLen - 1] == '*')
					{
						// Prefix match: "Physics.*" matches "Physics.Collision"
						return strncmp(mName, pattern, patternLen - 1) == 0;
					}

					// Exact match
					return strcmp(mName, pattern) == 0;
				}

				// Implicit conversion to StringCRC for use in hash tables
				operator StringCRC() const { return mId; }

			private:
				const char* mName;
				StringCRC mId;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Common Namespaces (can be used throughout codebase)
			//---------------------------------------------------------------------------------------------------------------------------------

			namespace LogNamespaces
			{
				inline LogNamespace Core()       { return LogNamespace("Core"); }
				inline LogNamespace Physics()    { return LogNamespace("Physics"); }
				inline LogNamespace Rendering()  { return LogNamespace("Rendering"); }
				inline LogNamespace Audio()      { return LogNamespace("Audio"); }
				inline LogNamespace Input()      { return LogNamespace("Input"); }
				inline LogNamespace Network()    { return LogNamespace("Network"); }
				inline LogNamespace AI()         { return LogNamespace("AI"); }
				inline LogNamespace UI()         { return LogNamespace("UI"); }
				inline LogNamespace Gameplay()   { return LogNamespace("Gameplay"); }
				inline LogNamespace Animation()  { return LogNamespace("Animation"); }
			}
		}
	}
}

#endif // DIA_LOG_NAMESPACE_H
