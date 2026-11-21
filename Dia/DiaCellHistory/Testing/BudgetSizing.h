#pragma once

// Test helper (Testing/ convention): pure budget arithmetic so tests can size a budget to hold exactly N samples.
// Per-cell tier: a tracked float layer costs 2 bytes per cell per sample (quantised), a category layer 1 byte.
// Aggregate tier (spec numbers): float layer 12 bytes (3 x float32) per group per sample; category layer 2 bytes
// (uint16) per enum value per group per sample; plus 2 bytes per group for group size.

#include <stdint.h>

namespace Dia::CellHistory::Testing
{
	inline uint64_t BytesPerSample(uint32_t floatTrackedCount, uint32_t categoryTrackedCount, uint32_t cellCount)
	{
		return uint64_t(floatTrackedCount) * 2u * cellCount + uint64_t(categoryTrackedCount) * 1u * cellCount;
	}

	// Whole samples that fit in the budget (floor); 0 when bytesPerSample is 0.
	inline uint64_t WindowLengthFor(uint64_t budgetBytes, uint64_t bytesPerSample)
	{
		return bytesPerSample == 0 ? 0 : budgetBytes / bytesPerSample;
	}

	inline uint64_t BudgetBytesForWindow(uint64_t window, uint64_t bytesPerSample)
	{
		return window * bytesPerSample;
	}

	// categoryEnumValueTotal = sum of enum value counts over all category layers.
	inline uint64_t AggregateRowBytes(uint32_t floatLayers, uint32_t categoryEnumValueTotal, uint32_t groupCount)
	{
		return uint64_t(groupCount) * (uint64_t(floatLayers) * 12u + uint64_t(categoryEnumValueTotal) * 2u + 2u);
	}
}
