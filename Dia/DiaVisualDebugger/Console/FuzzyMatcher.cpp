////////////////////////////////////////////////////////////////////////////////
// Filename: FuzzyMatcher.cpp
// Description: Case-insensitive fuzzy string matching and ranking.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/FuzzyMatcher.h>

#include <cstring>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				char Lower(char c)
				{
					return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
				}

				int CompareNoCase(const char* a, const char* b)
				{
					while (*a && *b)
					{
						const char ca = Lower(*a);
						const char cb = Lower(*b);
						if (ca != cb)
						{
							return ca < cb ? -1 : 1;
						}
						++a;
						++b;
					}
					if (*a == *b) return 0;
					return *a ? 1 : -1;
				}

				void SetMask(unsigned char* mask, unsigned maskSize, unsigned pos)
				{
					if (mask && pos < maskSize)
					{
						mask[pos] = 1;
					}
				}
			}

			MatchRank FuzzyMatcher::Match(const char* query, const char* candidate,
										  unsigned char* outMatchedMask, unsigned maskSize)
			{
				if (outMatchedMask)
				{
					std::memset(outMatchedMask, 0, maskSize);
				}
				if (!query) query = "";
				if (!candidate) candidate = "";

				const size_t qLen = std::strlen(query);
				const size_t cLen = std::strlen(candidate);
				if (qLen == 0)
				{
					return MatchRank::Prefix;
				}
				if (qLen > cLen)
				{
					return MatchRank::None;
				}

				// Substring search (covers exact and prefix as position 0).
				for (size_t start = 0; start + qLen <= cLen; ++start)
				{
					size_t i = 0;
					while (i < qLen && Lower(query[i]) == Lower(candidate[start + i]))
					{
						++i;
					}
					if (i == qLen)
					{
						for (size_t k = 0; k < qLen; ++k)
						{
							SetMask(outMatchedMask, maskSize, static_cast<unsigned>(start + k));
						}
						if (start == 0)
						{
							return qLen == cLen ? MatchRank::Exact : MatchRank::Prefix;
						}
						return MatchRank::Contains;
					}
				}

				// Subsequence: greedy in-order scan.
				size_t qi = 0;
				for (size_t ci = 0; ci < cLen && qi < qLen; ++ci)
				{
					if (Lower(query[qi]) == Lower(candidate[ci]))
					{
						SetMask(outMatchedMask, maskSize, static_cast<unsigned>(ci));
						++qi;
					}
				}
				if (qi != qLen)
				{
					if (outMatchedMask)
					{
						std::memset(outMatchedMask, 0, maskSize);
					}
					return MatchRank::None;
				}
				return MatchRank::Subsequence;
			}

			unsigned FuzzyMatcher::Rank(const char* query, const char* const* candidates,
										unsigned candidateCount, unsigned* outIndices, unsigned maxOut)
			{
				if (!outIndices || maxOut == 0 || !candidates)
				{
					return 0;
				}

				// Insertion into a sorted output of at most maxOut entries; ties keep input order.
				unsigned count = 0;
				for (unsigned i = 0; i < candidateCount; ++i)
				{
					const MatchRank rank = Match(query, candidates[i], nullptr, 0);
					if (rank == MatchRank::None)
					{
						continue;
					}

					// Find insertion position: after every entry that sorts before-or-equal.
					unsigned pos = count;
					while (pos > 0)
					{
						const unsigned prev = outIndices[pos - 1];
						const MatchRank prevRank = Match(query, candidates[prev], nullptr, 0);
						bool prevFirst;
						if (prevRank != rank)
						{
							prevFirst = prevRank > rank;
						}
						else
						{
							prevFirst = CompareNoCase(candidates[prev], candidates[i]) <= 0;
						}
						if (prevFirst) break;
						--pos;
					}

					if (pos >= maxOut)
					{
						continue;
					}
					const unsigned last = (count < maxOut) ? count : maxOut - 1;
					for (unsigned k = last; k > pos; --k)
					{
						outIndices[k] = outIndices[k - 1];
					}
					outIndices[pos] = i;
					if (count < maxOut) ++count;
				}
				return count;
			}
		}
	}
}
