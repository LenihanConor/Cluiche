#pragma once

// Test helper for IViewValueSource (Testing/ convention): settable live flag, epoch, values, presentation; counts calls.

#include <DiaCellLayers/IViewValueSource.h>

#include <string.h>
#include <vector>

namespace Dia::CellLayers::Testing
{
	class StubViewValueSource : public IViewValueSource
	{
	public:
		bool					live = false;
		uint64_t				epoch = 1;
		bool					hasData = true;			// GetValues result
		std::vector<float>		values;
		bool					hasPresentation = false;
		ViewPresentation		presentation;
		uint32_t				getValuesCalls = 0;

		bool GetValues(const CellLayerDesc&, float* out, uint32_t n) override
		{
			++getValuesCalls;
			if (!hasData) { return false; }
			for (uint32_t i = 0; i < n; ++i) { out[i] = i < values.size() ? values[i] : 0.0f; }
			return true;
		}
		bool		IsLive() const override { return live; }
		uint64_t	ViewEpoch() const override { return epoch; }
		bool GetPresentation(const CellLayerDesc&, ViewPresentation& out) override
		{
			if (!hasPresentation) { return false; }
			out = presentation;
			return true;
		}

		void SetPrefix(const char* p) { strncpy(presentation.unitPrefix, p, sizeof(presentation.unitPrefix) - 1); }
	};
}
