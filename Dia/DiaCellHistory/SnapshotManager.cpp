#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/ISnapshotStore.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace Dia::CellHistory
{
	namespace
	{
		void Copy(char* dst, size_t cap, const char* src) { std::snprintf(dst, cap, "%s", src ? src : ""); }
	}

	SnapshotManager::SnapshotManager(CellHistoryRecorder& recorder, Dia::CellLayers::CellLayerRegistry& registry,
		const Dia::ValueSchema::ValueSchemaService* schema, ISnapshotStore& store)
		: mRecorder(recorder), mRegistry(registry), mSchema(schema), mStore(store)
	{
	}

	void SnapshotManager::SetWorld(const char* world) { Copy(mWorld, sizeof(mWorld), world); }

	void SnapshotManager::Refuse(TakeResult& out, const char* msg) const
	{
		out.ok = false;
		out.pending = false;
		Copy(out.error, sizeof(out.error), msg);
	}

	SnapshotRecord* SnapshotManager::SlotFor(const char* name)
	{
		for (SnapshotRecord& r : mRecords) { if (r.mUsed && std::strcmp(r.mName, name) == 0) { return &r; } }
		for (SnapshotRecord& r : mRecords) { if (!r.mUsed) { return &r; } }
		for (SnapshotRecord& r : mRecords) { if (r.mStatus == SnapshotStatus::Failed) { return &r; } }
		return &mRecords[0];	// unreachable in practice: the cap keeps at most 16 ready records
	}

	bool SnapshotManager::Take(const char* name, bool overwrite, TakeResult& out)
	{
		out = TakeResult();
		char err[192] = {};
		if (!CheckTake(name, overwrite, mStore, mGate, err, sizeof(err))) { Refuse(out, err); return false; }
		if (mRecorder.SampleCount() == 0) { Refuse(out, "no samples recorded yet"); return false; }

		const int64_t now = mTimeFn != nullptr ? mTimeFn(mTimeUser) : int64_t(std::time(nullptr));
		bool built = false;
		{
			DIA_PROFILE_SCOPE("cellhistory.snapshot_take", ::Dia::Observation::Profile::Category::kDiaGraphics);
			mPending.Clear();
			built = SnapshotCellFile::BuildFromLive(mRecorder, mRegistry, mSchema, name, now, mWorld, mPending, err, sizeof(err));
		}
		if (!built)
		{
			DIA_LOG_ERROR("HIS", "cell history snapshot '%s': capture failed: %s", name, err);
			Refuse(out, err);
			return false;
		}
		if (!mGate.TryBegin(err, sizeof(err))) { Refuse(out, err); return false; }

		SnapshotRecord* rec = SlotFor(name);
		*rec = SnapshotRecord();
		rec->mUsed = true;
		Copy(rec->mName, sizeof(rec->mName), name);
		rec->mStatus = SnapshotStatus::Pending;
		rec->mSample = mPending.mHeader.mSample;
		Copy(rec->mLabel, sizeof(rec->mLabel), mPending.mHeader.mLabel);
		rec->mCreatedUnix = now;
		Copy(rec->mWorld, sizeof(rec->mWorld), mWorld);
		rec->mRunId = mPending.mHeader.mRunId;
		mPendingRecord = rec;

		DIA_LOG_INFO("HIS", "cell history snapshot '%s': take requested at sample %u", name, rec->mSample);
		mStore.RequestTake(name, &SnapshotManager::OnTakeDone, this);
		out.ok = true;
		out.pending = true;
		return true;
	}

	void SnapshotManager::OnTakeDone(bool ok, const char* message, void* user)
	{
		static_cast<SnapshotManager*>(user)->FinishTake(ok, message);
	}

	void SnapshotManager::FinishTake(bool ok, const char* message)
	{
		SnapshotRecord* rec = mPendingRecord;
		mPendingRecord = nullptr;
		if (rec == nullptr) { mGate.End(); return; }

		auto fail = [&](const char* reason)
		{
			rec->mStatus = SnapshotStatus::Failed;
			Copy(rec->mReason, sizeof(rec->mReason), reason);
		};

		if (!ok)
		{
			// A failed store may have left a partial state; the old cell file (overwrite) is untouched.
			DIA_LOG_ERROR("HIS", "cell history snapshot '%s': store failed: %s", rec->mName, message ? message : "");
			if (mStore.Exists(rec->mName)) { mStore.Delete(rec->mName); }
			fail(message ? message : "store failed");
		}
		else
		{
			char path[512] = {};
			char err[192] = {};
			bool written = false;
			if (!SnapshotCellFile::Path(mStore.Folder(), rec->mName, path, sizeof(path)))
			{
				std::snprintf(err, sizeof(err), "cannot build the cell file path");
			}
			else
			{
				DIA_PROFILE_SCOPE("cellhistory.snapshot_write", ::Dia::Observation::Profile::Category::kDiaGraphics);
				written = SnapshotCellFile::WriteAtomic(mPending, path, err, sizeof(err));
			}
			if (written)
			{
				std::error_code ec;
				const auto size = std::filesystem::file_size(path, ec);
				rec->mBytes = ec ? 0 : uint64_t(size);
				rec->mStatus = SnapshotStatus::Ready;
				rec->mReason[0] = '\0';
				DIA_LOG_INFO("HIS", "cell history snapshot '%s': taken at sample %u (%llu bytes)", rec->mName, rec->mSample, (unsigned long long)rec->mBytes);
			}
			else
			{
				DIA_LOG_ERROR("HIS", "cell history snapshot '%s': cell file write failed: %s", rec->mName, err);
				mStore.Delete(rec->mName);		// remove the store's half: both or neither
				fail(err);
			}
		}
		mPending.Clear();
		mGate.End();
	}

	// ---- restore -------------------------------------------------------------------------------------------

	void SnapshotManager::RefuseRestore(RestoreResult& out, const char* msg) const
	{
		out.ok = false;
		out.pending = false;
		Copy(out.error, sizeof(out.error), msg);
	}

	uint64_t SnapshotManager::NextResetSeed()
	{
		if (mSeedFn != nullptr) { return mSeedFn(mSeedUser); }
		const int64_t now = mTimeFn != nullptr ? mTimeFn(mTimeUser) : int64_t(std::time(nullptr));
		return uint64_t(now) + (++mResetCounter) * 0x9E3779B97F4A7C15ull;
	}

	bool SnapshotManager::Restore(const char* name, RestoreResult& out)
	{
		out = RestoreResult();
		char err[192] = {};
		if (!IsValidSnapshotName(name))
		{
			RefuseRestore(out, "invalid snapshot name (1 to 32 characters from A-Z a-z 0-9 _ -)");
			return false;
		}
		if (mGate.InFlight())
		{
			mGate.TryBegin(err, sizeof(err));		// fails and fills the standard message; changes nothing
			RefuseRestore(out, err);
			return false;
		}

		char path[512] = {};
		if (!SnapshotCellFile::Path(mStore.Folder(), name, path, sizeof(path)))
		{
			RefuseRestore(out, "cannot build the cell file path");
			return false;
		}
		std::error_code ec;
		const bool hasState = mStore.Exists(name);
		const bool hasCells = std::filesystem::exists(path, ec);
		if (!hasState || !hasCells)
		{
			if (!hasState && !hasCells) { std::snprintf(err, sizeof(err), "snapshot '%s' not found", name); }
			else if (!hasState) { std::snprintf(err, sizeof(err), "snapshot '%s': state missing (the game store has no state for it)", name); }
			else { std::snprintf(err, sizeof(err), "snapshot '%s': cell file missing", name); }
			RefuseRestore(out, err);
			return false;
		}

		SnapshotHeader header;
		SnapshotGridInfo grid;
		if (!SnapshotCellFile::ReadHeaderOnly(path, header, grid, err, sizeof(err)))
		{
			char msg[192] = {};
			std::snprintf(msg, sizeof(msg), "snapshot '%s': cell file unreadable: %.120s", name, err);
			RefuseRestore(out, msg);
			return false;
		}

		const Dia::Graphics::CellGridDesc* active = mRecorder.ActiveGrid();
		const bool sameGrid = active != nullptr && grid.mWidth == uint32_t(active->width) && grid.mHeight == uint32_t(active->height)
			&& std::strcmp(grid.mId, active->id.AsChar()) == 0;
		if (!sameGrid)
		{
			std::snprintf(err, sizeof(err), "snapshot is for a different grid (%u x %u); load that world first", grid.mWidth, grid.mHeight);
			RefuseRestore(out, err);
			return false;
		}

		if (!mGate.TryBegin(err, sizeof(err))) { RefuseRestore(out, err); return false; }
		mRestoreHeader = header;
		Copy(mRestoreName, sizeof(mRestoreName), name);
		DIA_LOG_INFO("HIS", "cell history snapshot '%s': restore requested (sample %u)", name, header.mSample);
		mStore.RequestRestore(name, &SnapshotManager::OnRestoreDone, this);
		out.ok = true;
		out.pending = true;
		return true;
	}

	void SnapshotManager::OnRestoreDone(bool ok, const char* message, void* user)
	{
		static_cast<SnapshotManager*>(user)->FinishRestore(ok, message);
	}

	void SnapshotManager::FinishRestore(bool ok, const char* message)
	{
		mLastRestore = RestoreOutcome();
		mLastRestore.mDone = true;
		Copy(mLastRestore.mName, sizeof(mLastRestore.mName), mRestoreName);

		if (!ok)
		{
			Copy(mLastRestore.mMessage, sizeof(mLastRestore.mMessage), message && message[0] ? message : "store failed");
			DIA_LOG_ERROR("HIS", "cell history snapshot '%s': restore failed in the store: %s (history unchanged)", mRestoreName, mLastRestore.mMessage);
			mGate.End();
			return;
		}

		DIA_PROFILE_SCOPE("cellhistory.snapshot_restore", ::Dia::Observation::Profile::Category::kDiaGraphics);
		const uint32_t before = mRecorder.SampleCount();
		const uint32_t sample = mRestoreHeader.mSample;
		const bool sameRun = mRestoreHeader.mRunId == mRecorder.RunId() && sample < before;
		mLastRestore.mOk = true;
		if (sameRun)
		{
			mRecorder.TruncateAfter(sample);
			const uint32_t dropped = before - (sample + 1);
			mLastRestore.mDropped = dropped;
			std::snprintf(mLastRestore.mMessage, sizeof(mLastRestore.mMessage), "dropped %u samples after sample %u", dropped, sample);
			if (dropped > 0) { DIA_LOG_WARNING("HIS", "cell history snapshot: restored '%s': dropped %u samples after sample %u", mRestoreName, dropped, sample); }
			DIA_LOG_INFO("HIS", "cell history snapshot '%s': restored (same run, dropped %u)", mRestoreName, dropped);
		}
		else
		{
			mRecorder.ResetHistory(NextResetSeed());
			mLastRestore.mReset = true;
			std::snprintf(mLastRestore.mMessage, sizeof(mLastRestore.mMessage), "history reset (snapshot from another run)");
			DIA_LOG_WARNING("HIS", "cell history snapshot: restored '%s': history reset (snapshot from another run)", mRestoreName);
			DIA_LOG_INFO("HIS", "cell history snapshot '%s': restored (history reset, new run id)", mRestoreName);
		}
		if (mView != nullptr) { mView->NotifyHistoryReset(); }
		mGate.End();
	}
}
