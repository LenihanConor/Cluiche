#pragma once

namespace Dia
{
	namespace API
	{
		// Shared parameter description used by DiaAPI commands and editor actions.
		// First four members keep legacy aggregate initialisation compiling.
		struct ValueParam
		{
			const char* name        = nullptr;  // e.g. "path"
			const char* type        = nullptr;  // "string" | "int" | "float" | "bool" | "enum"
			bool        required    = false;
			const char* description = nullptr;

			// Optional metadata
			const char* unit        = nullptr;  // e.g. "m"
			bool        hasRange    = false;    // min/max valid only when true
			float       min         = 0.0f;
			float       max         = 0.0f;
			const char* enumValues  = nullptr;  // "weather|ecology" for type "enum"
		};
	} // namespace API
} // namespace Dia
