#include <DiaCellHistory/SnapshotCellCache.h>

#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/ISnapshotStore.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>

namespace Dia::CellHistory
{
	namespace
	{
		void SetErr(char* err, uint32_t cap, const char* text)
		{
			if (err != nullptr && cap > 0) { std::snprintf(err, cap, "%s", text); }
		}
	}

	SnapshotCellCache::SnapshotCellCache(const ISnapshotStore& store) : mStore(store) {}

	SnapshotCellCache::~SnapshotCellCache() { Clear(); }

	void SnapshotCellCache::Drop(Entry& e)
	{
		delete e.mData;
		e.mData = nullptr;
		e.mName[0] = '\0';
		e.mStamp = 0;
	}

	int SnapshotCellCache::Find(const char* name) const
	{
		if (name == nullptr) { return -1; }
		for (uint32_t i = 0; i < kCapacity; ++i)
		{
			if (mEntries[i].mData != nullptr && std::strcmp(mEntries[i].mName, name) == 0) { return static_cast<int>(i); }
		}
		return -1;
	}

	uint32_t SnapshotCellCache::Count() const
	{
		uint32_t n = 0;
		for (const Entry& e : mEntries) { if (e.mData != nullptr) { ++n; } }
		return n;
	}

	bool SnapshotCellCache::Peek(const char* name) const { return Find(name) >= 0; }

	bool SnapshotCellCache::Exists(const char* name) const
	{
		if (!IsValidSnapshotName(name)) { return false; }
		char path[512] = {};
		if (!SnapshotCellFile::Path(mStore.Folder(), name, path, sizeof(path))) { return false; }
		std::error_code ec;
		return std::filesystem::exists(path, ec);
	}

	const SnapshotCellData* SnapshotCellCache::Get(const char* name, char* err, uint32_t errCapacity)
	{
		SetErr(err, errCapacity, "");
		if (!IsValidSnapshotName(name))
		{
			++mLoadFailures;
			SetErr(err, errCapacity, "invalid snapshot name (1 to 32 characters from A-Z a-z 0-9 _ -)");
			return nullptr;
		}
		const int hit = Find(name);
		if (hit >= 0)
		{
			++mHits;
			mEntries[hit].mStamp = ++mClock;
			return mEntries[hit].mData;
		}
		++mMisses;

		DIA_PROFILE_SCOPE("cellhistory.snapshot_cache_load", ::Dia::Observation::Profile::Category::kDiaGraphics);
		char path[512] = {};
		char reason[160] = {};
		SnapshotCellData* data = new (std::nothrow) SnapshotCellData();
		bool loaded = false;
		if (data == nullptr)
		{
			std::snprintf(reason, sizeof(reason), "out of memory");
		}
		else if (!SnapshotCellFile::Path(mStore.Folder(), name, path, sizeof(path)))
		{
			std::snprintf(reason, sizeof(reason), "cannot build the cell file path");
		}
		else
		{
			loaded = SnapshotCellFile::Read(path, *data, reason, sizeof(reason));
		}
		if (!loaded)
		{
			delete data;
			++mLoadFailures;
			if (std::strcmp(mFailedName, name) != 0)
			{
				std::snprintf(mFailedName, sizeof(mFailedName), "%s", name);
				DIA_LOG_ERROR("HIS", "cell history snapshot '%s': cannot load the cell file for a delta: %s", name, reason);
			}
			SetErr(err, errCapacity, reason);
			return nullptr;
		}

		if (std::strcmp(mFailedName, name) == 0) { mFailedName[0] = '\0'; }
		++mLoads;
		// Take a free slot, else evict the least recently used.
		uint32_t slot = 0;
		for (uint32_t i = 0; i < kCapacity; ++i)
		{
			if (mEntries[i].mData == nullptr) { slot = i; break; }
			if (mEntries[i].mStamp < mEntries[slot].mStamp) { slot = i; }
		}
		Drop(mEntries[slot]);
		mEntries[slot].mData = data;
		std::snprintf(mEntries[slot].mName, sizeof(mEntries[slot].mName), "%s", name);
		mEntries[slot].mStamp = ++mClock;
		return data;
	}

	void SnapshotCellCache::Invalidate(const char* name)
	{
		if (name == nullptr) { return; }
		const int i = Find(name);
		if (i >= 0) { Drop(mEntries[i]); }
		if (std::strcmp(mFailedName, name) == 0) { mFailedName[0] = '\0'; }
		if (mView != nullptr) { mView->NotifySnapshotChanged(name); }
	}

	void SnapshotCellCache::Clear()
	{
		for (Entry& e : mEntries) { Drop(e); }
		mFailedName[0] = '\0';
	}

	bool SnapshotCellCache::GridMatches(const SnapshotCellData& data, const Dia::Graphics::CellGridDesc& grid)
	{
		return data.mGrid.mWidth == static_cast<uint32_t>(grid.width) && data.mGrid.mHeight == static_cast<uint32_t>(grid.height)
			&& std::strcmp(data.mGrid.mId, grid.id.AsChar()) == 0;
	}
}
