////////////////////////////////////////////////////////////////////////////////
// Filename: PaletteIndex.h
// Description: Searchable snapshot of registered commands and debug domains for
//              the Palette, plus the action an item resolves to when chosen.
//              Pure logic; Build() snapshots the live registries.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			enum class PaletteItemType { Command, Domain };

			struct PaletteItem
			{
				PaletteItemType type;
				char name[64];
				char description[128];
				char category[32];	// command category, or domain group
				char example[128];	// commands only, may be empty
				char domainId[64];	// domains only: GetDomainId(); NOT the panel card key (that is `name`, the display name)
			};

			class PaletteIndex
			{
			public:
				static const int kMaxResults = 50;
				static const int kTypeCommands = 1;
				static const int kTypeDomains = 2;

				PaletteIndex();
				~PaletteIndex();
				PaletteIndex(const PaletteIndex&) = delete;
				PaletteIndex& operator=(const PaletteIndex&) = delete;

				// Snapshots all registered commands and all domains; replaces previous contents.
				void Build(const DiaDebugDomainRegistry& domains);

				// typeFilterMask: bit0 = commands, bit1 = domains. Fills out (at most 50) with item
				// indices ranked name-match first (exact > prefix > contains > subsequence, ties
				// alphabetical), then description/category matches. Returns the total match count.
				// Empty query matches everything alphabetically.
				int Search(const char* query, int typeFilterMask,
						   Dia::Core::Containers::DynamicArrayC<int, 50>& out) const;

				const PaletteItem& GetItem(int index) const;
				int GetItemCount() const;
				int CountByType(PaletteItemType type) const;

			private:
				PaletteItem* mItems;
				int mCount;
			};

			struct PaletteAction
			{
				enum Kind { FillConsole, RevealDomain } kind;
				char text[160];
			};

			// Command -> FillConsole (name plus the example's arguments when the example starts with
			// the name); Domain -> RevealDomain with the domain id.
			PaletteAction ResolvePaletteAction(const PaletteItem& item);
		}
	}
}

#endif // DIA_DEBUG
