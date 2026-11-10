#pragma once

// Test helper (Testing/ convention): a small grid plus settable layers, each backed by a std::vector<float>.
// Produces CellLayerDesc entries whose read/version delegates point back into this object, so the StubGrid must
// outlive any registry it is registered with. Counts read calls per layer (for version-gate tests).
// Category layers are floats holding integer ids.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaGraphics/CellField/CellGridDesc.h>

#include <memory>
#include <string>
#include <vector>

namespace Dia::CellHistory::Testing
{
	class StubGrid
	{
	public:
		explicit StubGrid(uint16_t width = 8, uint16_t height = 6, const char* topology = "square")
		{
			mGrid.id = Dia::Core::StringCRC("test.stubgrid");
			mGrid.topology = Dia::Core::StringCRC(topology);
			mGrid.width = width;
			mGrid.height = height;
			mGrid.cellSize = 1.0f;
			mCentres.assign(size_t(width) * height * 2, 0.0f);
			mNeighbours.assign(size_t(width) * height * 6, -1);
			mGrid.centresXY = mCentres.data();
			mGrid.neighbours = mNeighbours.data();
		}
		StubGrid(const StubGrid&) = delete;
		StubGrid& operator=(const StubGrid&) = delete;

		const Dia::Graphics::CellGridDesc&	Grid() const { return mGrid; }
		// Changes the topology id (e.g. "hex.flat.oddq" so a CellField accepts the grid). Call before registering layers.
		void		SetTopology(const char* topology) { mGrid.topology = Dia::Core::StringCRC(topology); }
		uint32_t	CellCount() const { return uint32_t(mGrid.width) * mGrid.height; }
		uint32_t	LayerCount() const { return uint32_t(mLayers.size()); }

		// Layers start filled with the initial value. Id doubles as the schema key. Returns the layer index.
		uint32_t AddFloatLayer(const char* id, float initial = 0.0f, const char* group = "test")
		{
			return Add(id, group, Dia::CellLayers::CellValueKind::Float, initial);
		}
		uint32_t AddCategoryLayer(const char* id, int initialCategory = 0, const char* group = "test")
		{
			return Add(id, group, Dia::CellLayers::CellValueKind::Category, float(initialCategory));
		}

		void Set(const char* layerId, uint32_t cell, float value)
		{
			Layer* l = Find(layerId);
			if (l != nullptr && cell < l->values.size()) { l->values[cell] = value; }
		}
		void SetAll(const char* layerId, float value)
		{
			Layer* l = Find(layerId);
			if (l != nullptr) { for (float& v : l->values) { v = value; } }
		}
		float Get(const char* layerId, uint32_t cell) const
		{
			const Layer* l = Find(layerId);
			return (l != nullptr && cell < l->values.size()) ? l->values[cell] : 0.0f;
		}

		void		BumpVersion(const char* layerId) { if (Layer* l = Find(layerId)) { ++l->version; } }
		uint64_t	Version(const char* layerId) const { const Layer* l = Find(layerId); return l ? l->version : 0; }
		// Number of read() calls made through the descs since creation or ResetReadCounts.
		uint32_t	ReadCount(const char* layerId) const { const Layer* l = Find(layerId); return l ? l->reads : 0; }
		void		ResetReadCounts() { for (auto& l : mLayers) { l->reads = 0; } }

		// Set + bump version in one call (the usual "this layer changed" action).
		void SetAndBump(const char* layerId, uint32_t cell, float value) { Set(layerId, cell, value); BumpVersion(layerId); }

		bool Describe(uint32_t index, Dia::CellLayers::CellLayerDesc& out)
		{
			if (index >= mLayers.size()) { return false; }
			Layer* l = mLayers[index].get();
			out.id = Dia::Core::StringCRC(l->name.c_str());
			out.schemaKey = out.id;
			out.group = Dia::Core::Containers::String32(l->group.c_str());
			out.kind = l->kind;
			out.grid = &mGrid;
			out.read = fastdelegate::MakeDelegate(l, &Layer::Read);
			out.version = fastdelegate::MakeDelegate(l, &Layer::Version);
			return true;
		}

		// Registers every layer in add order; returns how many came back Ok. Call from the registry's owner PU.
		uint32_t RegisterAll(Dia::CellLayers::CellLayerRegistry& registry)
		{
			uint32_t ok = 0;
			for (uint32_t i = 0; i < mLayers.size(); ++i)
			{
				Dia::CellLayers::CellLayerDesc d;
				Describe(i, d);
				if (registry.Register(d) == Dia::CellLayers::CellRegisterResult::Ok) { ++ok; }
			}
			return ok;
		}

	private:
		struct Layer
		{
			std::string			name;
			std::string			group;
			Dia::CellLayers::CellValueKind	kind = Dia::CellLayers::CellValueKind::Float;
			std::vector<float>	values;
			uint64_t			version = 1;
			uint32_t			reads = 0;

			void		Read(float* out, uint32_t n)
			{
				++reads;
				for (uint32_t i = 0; i < n; ++i) { out[i] = i < values.size() ? values[i] : 0.0f; }
			}
			uint64_t	Version() { return version; }
		};

		uint32_t Add(const char* id, const char* group, Dia::CellLayers::CellValueKind kind, float initial)
		{
			auto l = std::make_unique<Layer>();
			l->name = id;
			l->group = group;
			l->kind = kind;
			l->values.assign(CellCount(), initial);
			mLayers.push_back(std::move(l));
			return uint32_t(mLayers.size() - 1);
		}
		Layer* Find(const char* id)
		{
			for (auto& l : mLayers) { if (l->name == id) { return l.get(); } }
			return nullptr;
		}
		const Layer* Find(const char* id) const
		{
			for (const auto& l : mLayers) { if (l->name == id) { return l.get(); } }
			return nullptr;
		}

		Dia::Graphics::CellGridDesc			mGrid;
		std::vector<float>					mCentres;		// zeros: enough for a CellField to accept the grid
		std::vector<int32_t>				mNeighbours;	// all -1
		std::vector<std::unique_ptr<Layer>>	mLayers;
	};
}
