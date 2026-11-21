#pragma once

// Test helpers for CellDetailRegistry (Testing/ convention, no GoogleTest): a stub provider that counts calls, records the
// last cell index and emits either N literal rows or a fixed set of known rows.

#include <DiaCellLayers/CellDetailRegistry.h>

#include <stdio.h>

namespace Dia::CellLayers::Testing
{
	struct StubDetailProvider
	{
		uint32_t	calls = 0;
		uint32_t	lastCell = 0xFFFFFFFFu;
		uint32_t	rowsToEmit = 0;			// emits Row("row<i>", "v<i>") this many times
		const char*	sectionTitle = nullptr;	// optional Section() first
		Dia::Core::StringCRC	valueKey;	// optional RowValue(valueKey, valueToEmit) when non-zero
		float		valueToEmit = 0.0f;

		void Provide(uint32_t cellIndex, CellDetailBuilder& b)
		{
			++calls;
			lastCell = cellIndex;
			if (sectionTitle) { b.Section(sectionTitle); }
			if (valueKey != Dia::Core::StringCRC::kZero) { b.RowValue(valueKey, valueToEmit); }
			for (uint32_t i = 0; i < rowsToEmit; ++i)
			{
				char l[16]; char t[16];
				snprintf(l, sizeof(l), "row%u", i);
				snprintf(t, sizeof(t), "v%u", i);
				b.Row(l, t);
			}
		}
		CellDetailProviderFn Delegate() { return fastdelegate::MakeDelegate(this, &StubDetailProvider::Provide); }
	};
}
