#pragma once

// Test helpers for LayerReader (Testing/ convention): a layer provider that counts read calls and has a bumpable version.

#include <DiaCellLayers/Testing/CellLayerTestSupport.h>

#include <vector>

namespace Dia::CellLayers::Testing
{
	class CountingLayerProvider
	{
	public:
		std::vector<float>	values;
		uint64_t			version = 1;
		uint32_t			readCalls = 0;

		void		Read(float* out, uint32_t n)
		{
			++readCalls;
			for (uint32_t i = 0; i < n; ++i) { out[i] = i < values.size() ? values[i] : 0.0f; }
		}
		uint64_t	Version() { return version; }
		void		Bump() { ++version; }
	};

	inline CellLayerDesc MakeCountingLayer(const char* id, const Dia::Graphics::CellGridDesc* grid, CountingLayerProvider& provider,
		CellValueKind kind = CellValueKind::Float)
	{
		CellLayerDesc d = MakeLayer(id, grid, nullptr, kind);
		d.read = fastdelegate::MakeDelegate(&provider, &CountingLayerProvider::Read);
		d.version = fastdelegate::MakeDelegate(&provider, &CountingLayerProvider::Version);
		return d;
	}
}
