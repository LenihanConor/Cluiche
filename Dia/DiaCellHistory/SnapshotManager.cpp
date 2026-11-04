#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/ISnapshotStore.h>
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
}
