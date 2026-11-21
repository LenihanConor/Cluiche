#pragma once

namespace Dia
{
	namespace Editor
	{
		// Normalised key for comparing .diagame paths: absolute, '/' separators,
		// '.' and '..' resolved, lowercased (Windows paths are case-insensitive).
		// Never throws. out is always NUL-terminated (empty on null/empty input).
		void NormalizeGamePath(const char* in, char* out, unsigned int outSize);

		// True when both paths denote the same game. Null/empty never match.
		bool IsSameGamePath(const char* a, const char* b);
	}
}
