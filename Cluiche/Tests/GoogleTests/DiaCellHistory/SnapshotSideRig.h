#pragma once

// Shared fixture for the snapshot-side tests (Feature 4 task 4.9): a Rig with temp (float 0..100, tracked), biome (category,
// tracked) and rain (float, history.aggregate false: not recorded, so absent from snapshots), a real SnapshotManager over a FakeSnapshotStore so
// snapshots are real cell files, and one cache over the store.

#include "HistoryTestRig.h"
#include <DiaCellHistory/SnapshotCellCache.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>

#include <memory>
#include <string>

namespace SnapRig
{
	struct Fix
	{
		HistoryRig::Rig r;
		Dia::CellHistory::Testing::FakeSnapshotStore store;
		std::unique_ptr<Dia::CellHistory::SnapshotManager> mgr;
		std::unique_ptr<Dia::CellHistory::SnapshotCellCache> cache;

		// samples 0..count-1: temp[c] = 10*i + c; biome = 2 on cells 6..11 for i < 3, else 1.
		void Setup(int count = 5)
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			r.grid.AddFloatLayer("rain", 5.0f);
			ASSERT_TRUE(r.schema.Load({ HistoryRig::FloatEntry("temp", 0, 100), HistoryRig::EnumEntry("biome", 3), HistoryRig::FloatEntry("rain", 0, 10, "{\"aggregate\":false}") }));
			r.cfg.mTrackCount = 2;
			r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "temp");
			r.cfg.mTrack[1] = Dia::Core::Containers::String64("%s", "biome");
			r.Start(nullptr, false);
			for (int i = 0; i < count; ++i) { Sample(i); }
			mgr = std::make_unique<Dia::CellHistory::SnapshotManager>(*r.rec, r.registry, r.schema.service.get(), store);
			cache = std::make_unique<Dia::CellHistory::SnapshotCellCache>(store);
		}
		void Sample(int i)
		{
			for (uint32_t c = 0; c < 12; ++c)
			{
				r.grid.Set("temp", c, float(10 * i + c));
				r.grid.Set("biome", c, (i < 3 && c >= 6) ? 2.0f : 1.0f);
			}
			r.grid.BumpVersion("temp"); r.grid.BumpVersion("biome");
			r.Take();
		}
		void SetLiveTemp(float base)
		{
			for (uint32_t c = 0; c < 12; ++c) { r.grid.Set("temp", c, base + float(c)); }
			r.grid.BumpVersion("temp");
		}
		// Takes a real snapshot of the CURRENT live values.
		bool Snap(const char* name, bool overwrite = false)
		{
			Dia::CellHistory::TakeResult res;
			if (!mgr->Take(name, overwrite, res)) { ADD_FAILURE() << res.error; return false; }
			store.RunFrameBoundary();
			return true;
		}
		std::string Path(const char* name) const
		{
			char p[512];
			EXPECT_TRUE(Dia::CellHistory::SnapshotCellFile::Path(store.Folder(), name, p, sizeof(p)));
			return p;
		}
		const Dia::CellLayers::CellLayerDesc& Desc(const char* id) const
		{
			const Dia::CellLayers::CellLayerDesc* d = r.registry.Find(Dia::Core::StringCRC(id));
			EXPECT_NE(d, nullptr);
			return *d;
		}
	};
}
