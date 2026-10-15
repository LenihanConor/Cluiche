////////////////////////////////////////////////////////////////////////////////
// Filename: PaletteIndex.cpp
// Description: Palette snapshot, search and action resolution.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/PaletteIndex.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/FuzzyMatcher.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>

#include <algorithm>
#include <cstring>
#include <cctype>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				const int kMaxCommands = 256;
				const int kMaxDomains = 256;
				const int kCapacity = kMaxCommands + kMaxDomains;

				void CopyStr(char* dst, unsigned size, const char* src)
				{
					if (!src) src = "";
					std::strncpy(dst, src, size - 1);
					dst[size - 1] = '\0';
				}

				int CompareNoCase(const char* a, const char* b)
				{
					for (; *a && *b; ++a, ++b)
					{
						const int ca = std::tolower(static_cast<unsigned char>(*a));
						const int cb = std::tolower(static_cast<unsigned char>(*b));
						if (ca != cb) return ca < cb ? -1 : 1;
					}
					return (*a ? 1 : 0) - (*b ? 1 : 0);
				}

				struct Scored
				{
					int index;
					int nameRank;	// MatchRank of name, 0 if none
					int otherRank;	// best MatchRank of description/category (Contains or better)
				};
			}

			PaletteIndex::PaletteIndex() : mItems(new PaletteItem[kCapacity]), mCount(0) {}

			PaletteIndex::~PaletteIndex() { delete[] mItems; }

			void PaletteIndex::Build(const DiaDebugDomainRegistry& domains)
			{
				mCount = 0;

				Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfo, 256> infos;
				Dia::API::ListCommandInfos(infos);
				for (int i = 0; i < static_cast<int>(infos.Size()) && mCount < kMaxCommands; ++i)
				{
					PaletteItem& it = mItems[mCount++];
					std::memset(&it, 0, sizeof(it));
					it.type = PaletteItemType::Command;
					CopyStr(it.name, sizeof(it.name), infos[i].name.AsChar());
					CopyStr(it.description, sizeof(it.description), infos[i].description);
					CopyStr(it.category, sizeof(it.category), infos[i].category.AsChar());
					CopyStr(it.example, sizeof(it.example), infos[i].example);
				}

				int domainCount = 0;
				domains.VisitAll([&](IDebugDomain& d)
				{
					if (domainCount >= kMaxDomains) return;
					++domainCount;
					PaletteItem& it = mItems[mCount++];
					std::memset(&it, 0, sizeof(it));
					it.type = PaletteItemType::Domain;
					CopyStr(it.name, sizeof(it.name), d.GetDisplayName());
					CopyStr(it.description, sizeof(it.description), d.GetDescription());
					CopyStr(it.category, sizeof(it.category), d.GetGroup().AsChar());
					CopyStr(it.domainId, sizeof(it.domainId), d.GetDomainId().AsChar());
				});
			}

			int PaletteIndex::Search(const char* query, int typeFilterMask,
									 Dia::Core::Containers::DynamicArrayC<int, 50>& out) const
			{
				out.RemoveAll();
				if (!query) query = "";

				Scored scored[kCapacity];
				int n = 0;
				for (int i = 0; i < mCount; ++i)
				{
					const PaletteItem& it = mItems[i];
					const int bit = it.type == PaletteItemType::Command ? kTypeCommands : kTypeDomains;
					if ((typeFilterMask & bit) == 0) continue;

					int nameRank = static_cast<int>(FuzzyMatcher::Match(query, it.name, nullptr, 0));
					int otherRank = 0;
					if (nameRank == 0)
					{
						const int d = static_cast<int>(FuzzyMatcher::Match(query, it.description, nullptr, 0));
						const int c = static_cast<int>(FuzzyMatcher::Match(query, it.category, nullptr, 0));
						otherRank = std::max(d, c);
						if (otherRank < static_cast<int>(MatchRank::Contains)) continue;
					}
					scored[n++] = Scored{ i, nameRank, otherRank };
				}

				std::stable_sort(scored, scored + n, [this](const Scored& a, const Scored& b)
				{
					if (a.nameRank != b.nameRank) return a.nameRank > b.nameRank;
					if (a.otherRank != b.otherRank) return a.otherRank > b.otherRank;
					return CompareNoCase(mItems[a.index].name, mItems[b.index].name) < 0;
				});

				for (int i = 0; i < n && i < kMaxResults; ++i)
				{
					out.Add(scored[i].index);
				}
				return n;
			}

			const PaletteItem& PaletteIndex::GetItem(int index) const { return mItems[index]; }

			int PaletteIndex::GetItemCount() const { return mCount; }

			int PaletteIndex::CountByType(PaletteItemType type) const
			{
				int c = 0;
				for (int i = 0; i < mCount; ++i)
				{
					if (mItems[i].type == type) ++c;
				}
				return c;
			}

			PaletteAction ResolvePaletteAction(const PaletteItem& item)
			{
				PaletteAction a;
				std::memset(&a, 0, sizeof(a));
				if (item.type == PaletteItemType::Domain)
				{
					a.kind = PaletteAction::RevealDomain;
					// The panel page keys domain cards by display name (the panel JSON is rooted at
					// GetDisplayName()), which differs from GetDomainId() for some domains (e.g.
					// "MessageBus" vs "Message Bus"). Reveal by the key the page actually uses.
					CopyStr(a.text, sizeof(a.text), item.name);
					return a;
				}

				a.kind = PaletteAction::FillConsole;
				CopyStr(a.text, sizeof(a.text), item.name);
				const size_t nameLen = std::strlen(item.name);
				if (item.example[0] != '\0' && std::strncmp(item.example, item.name, nameLen) == 0)
				{
					const char* rest = item.example + nameLen;
					if (*rest == ' ')
					{
						while (*rest == ' ') ++rest;
						if (*rest != 0)
						{
							std::strncat(a.text, " ", sizeof(a.text) - std::strlen(a.text) - 1);
							std::strncat(a.text, rest, sizeof(a.text) - std::strlen(a.text) - 1);
						}
					}
				}
				return a;
			}
		}
	}
}

#endif // DIA_DEBUG
