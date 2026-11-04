#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;
	using Dia::CellLayers::CellLayerDesc;
	using Dia::CellLayers::CellValueKind;

	namespace
	{
		const SampleHeader	sEmptyHeader;
		const uint64_t		kBytesPerMB = 1048576ull;

		uint64_t SplitMix64(uint64_t x)
		{
			x += 0x9E3779B97F4A7C15ull;
			x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
			x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
			return x ^ (x >> 31);
		}

		RecorderBudgets ResolveBudgets(const HistoryConfig& cfg, const RecorderBudgets* overrideBytes)
		{
			if (overrideBytes != nullptr) { return *overrideBytes; }
			RecorderBudgets b;
			b.mCellBytes = uint64_t(cfg.mCellBudgetMB) * kBytesPerMB;
			b.mAggregateBytes = uint64_t(cfg.mAggregateBudgetMB) * kBytesPerMB;
			b.mEventBytes = uint64_t(cfg.mEventBudgetMB) * kBytesPerMB;
			return b;
		}

		void NoopVisitor(const EventView&, void*) {}
	}

	uint32_t EventRange::Count() const { return mLog ? mLog->ForEachEvent(mFrom, mTo, &NoopVisitor, nullptr) : 0u; }

	CellHistoryRecorder::CellHistoryRecorder(Dia::CellLayers::CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema,
		const HistoryConfig& config, uint64_t runIdSeed, const RecorderBudgets* budgetOverride)
		: mRegistry(registry)
		, mSchema(schema)
		, mConfig(config)
		, mBudgets(ResolveBudgets(config, budgetOverride))
		, mRunId(SplitMix64(runIdSeed))
		, mAggregates(mBudgets.mAggregateBytes)
		, mEvents(mBudgets.mEventBytes)
	{
	}

	CellHistoryRecorder::~CellHistoryRecorder()
	{
		delete mRing;
		for (uint32_t i = 0; i < mRecordedCount; ++i) { delete[] mRecorded[i].mCache; }
		delete[] mMasks;
		delete[] mScratch;
		delete[] mHeaders;
	}

	// ---- event validation -----------------------------------------------------------------------------------

	void CellHistoryRecorder::WarnEventOnce(const HistoryEvent& e, const char* reason)
	{
		const uint32_t crc = e.type.Value();
		for (uint32_t i = 0; i < mWarnedTypeCount; ++i) { if (mWarnedTypes[i] == crc) { return; } }
		if (mWarnedTypeCount >= kMaxWarnedTypes) { ++mValidationSuppressed; return; }
		mWarnedTypes[mWarnedTypeCount++] = crc;
		DIA_LOG_WARNING("HIS", "cell history: event '%s' %s; further problems with this type are not reported", e.type.AsChar(), reason);
	}

#ifdef DIA_DEBUG
	namespace
	{
		bool MatchesType(const Json::Value& v, Dia::ValueSchema::ValueType t)
		{
			using Dia::ValueSchema::ValueType;
			switch (t)
			{
			case ValueType::Float:
			case ValueType::Int:		return v.isNumeric();
			case ValueType::Bool:		return v.isBool();
			case ValueType::Enum:
			case ValueType::String:
			case ValueType::Ref:		return v.isString();
			case ValueType::RefList:	return v.isArray();
			case ValueType::Map:		return v.isObject();
			}
			return false;
		}
	}

	void CellHistoryRecorder::ValidateEvent(const HistoryEvent& e)
	{
		if (mSchema == nullptr) { return; }
		const Dia::ValueSchema::ValueInfo* info = mSchema->FindEvent(e.type);
		if (info == nullptr) { WarnEventOnce(e, "is not declared in the value schema"); return; }
		if (e.payloadJson == nullptr || e.payloadJson[0] == '\0') { return; }
		const size_t len = strnlen(e.payloadJson, EventLog::kMaxPayloadBytes + 1);
		if (len > EventLog::kMaxPayloadBytes) { return; }	// the EventLog rejects it with an Error

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(e.payloadJson, e.payloadJson + len, root, false) || !root.isObject())
		{
			WarnEventOnce(e, "has a payload that is not a JSON object");
			return;
		}
		const Json::Value::Members names = root.getMemberNames();
		for (size_t n = 0; n < names.size(); ++n)
		{
			const Dia::ValueSchema::PayloadField* field = nullptr;
			for (uint32_t f = 0; f < info->mPayload.Size(); ++f)
			{
				if (names[n] == info->mPayload[f].mName.AsCStr()) { field = &info->mPayload[f]; break; }
			}
			if (field == nullptr) { WarnEventOnce(e, "has a payload field that its schema does not declare"); return; }
			const Json::Value& v = root[names[n]];
			if (!MatchesType(v, field->mType)) { WarnEventOnce(e, "has a payload field of the wrong type"); return; }
			if (field->mType == Dia::ValueSchema::ValueType::Enum && info->mVocabulary.Size() > 0)
			{
				bool found = false;
				for (uint32_t k = 0; k < info->mVocabulary.Size() && !found; ++k) { found = (v.asString() == info->mVocabulary[k].mId.AsCStr()); }
				if (!found) { WarnEventOnce(e, "has a payload value outside its vocabulary"); return; }
			}
		}
	}
#else
	void CellHistoryRecorder::ValidateEvent(const HistoryEvent&) {}
#endif

	EmitResult CellHistoryRecorder::Emit(const HistoryEvent& e)
	{
		DIA_PROFILE_SCOPE("cellhistory.emit", ::Dia::Observation::Profile::Category::kDiaGraphics);
		ValidateEvent(e);
		return mEvents.Emit(e);
	}

	// ---- layout ---------------------------------------------------------------------------------------------

	bool CellHistoryRecorder::ResolveGrid()
	{
		if (mGrid != nullptr) { return true; }
		const uint32_t gridCount = mRegistry.GridCount();
		if (gridCount == 0) { return false; }
		const Dia::Graphics::CellGridDesc* grid = nullptr;
		if (mConfig.mGrid.Length() > 0)
		{
			const StringCRC wanted(mConfig.mGrid.AsCStr());
			for (uint32_t i = 0; i < gridCount; ++i)
			{
				if (mRegistry.GridAt(i)->id == wanted) { grid = mRegistry.GridAt(i); break; }
			}
			if (grid == nullptr)
			{
				if (!mGridErrorLogged) { DIA_LOG_ERROR("HIS", "cell history: configured grid '%s' is not registered; nothing is recorded", mConfig.mGrid.AsCStr()); }
				mGridErrorLogged = true;
				return false;
			}
		}
		else
		{
			grid = mRegistry.GridAt(0);
		}
		const uint32_t cells = uint32_t(grid->width) * uint32_t(grid->height);
		if (cells == 0)
		{
			if (!mGridErrorLogged) { DIA_LOG_ERROR("HIS", "cell history: recorded grid has no cells"); }
			mGridErrorLogged = true;
			return false;
		}
		mGrid = grid;
		mCellCount = cells;
		mRing = new CellRing(cells, mBudgets.mCellBytes);
		mScratch = new float[cells];
		return true;
	}

	bool CellHistoryRecorder::Seen(uint32_t crc) const
	{
		for (uint32_t i = 0; i < mSeenCount; ++i) { if (mSeen[i] == crc) { return true; } }
		return false;
	}

	void CellHistoryRecorder::MarkSeen(uint32_t crc)
	{
		if (mSeenCount < kMaxRecordedLayers) { mSeen[mSeenCount++] = crc; }
	}

	int CellHistoryRecorder::FindRecorded(StringCRC id) const
	{
		for (uint32_t i = 0; i < mRecordedCount; ++i) { if (mRecorded[i].mId == id) { return int(i); } }
		return -1;
	}

	void CellHistoryRecorder::ResolveLayers()
	{
		const uint32_t n = mRegistry.LayerCountForGrid(mGrid);
		for (uint32_t i = 0; i < n; ++i)
		{
			const CellLayerDesc* desc = mRegistry.LayerOfGrid(mGrid, i);
			if (desc == nullptr || Seen(desc->id.Value())) { continue; }
			AddLayer(*desc);
		}
	}

	void CellHistoryRecorder::AddLayer(const CellLayerDesc& desc)
	{
		MarkSeen(desc.id.Value());
		const Dia::ValueSchema::ValueInfo* info = mSchema ? mSchema->Find(desc.schemaKey) : nullptr;
		const bool category = desc.kind == CellValueKind::Category;
		const bool aggregate = (info == nullptr) || HistoryAggregate(*info);
		const bool isGroup = info != nullptr && category && HistoryGroup(*info);
		if (!aggregate && !isGroup) { return; }	// excluded by the schema: not recorded

		if (mFrozen)
		{
			DIA_LOG_WARNING("HIS", "cell history: layer '%s' was registered after the first sample; it is not recorded", desc.id.AsChar());
			++mSkippedLayers;
			return;
		}
		const uint32_t cap = std::min<uint32_t>(std::max<uint32_t>(mConfig.mMaxAggregateLayers, 1u), HistoryConfig::kHardMaxAggregateLayers);
		if (aggregate && mAggregatedCount >= cap)
		{
			if (!mCapWarned)
			{
				DIA_LOG_WARNING("HIS", "cell history: more than %u aggregate layers; '%s' and any further layers are not recorded", cap, desc.id.AsChar());
				mCapWarned = true;
			}
			++mSkippedLayers;
			return;
		}
		if (mRecordedCount >= kMaxRecordedLayers) { ++mSkippedLayers; return; }

		uint32_t enumCount = 0;
		if (category)
		{
			enumCount = (info != nullptr && info->mEnumValues.Size() > 0)
				? std::min<uint32_t>(info->mEnumValues.Size(), AggregateStore::kMaxEnumCount) : AggregateStore::kMaxEnumCount;
		}
		if (aggregate && !mAggregates.AddLayer(desc.id, category ? AggregateKind::Category : AggregateKind::Float, enumCount))
		{
			++mSkippedLayers;
			return;
		}

		Recorded& r = mRecorded[mRecordedCount];
		r = Recorded();
		r.mId = desc.id;
		r.mAggregated = aggregate;
		r.mIsGroup = isGroup;
		r.mCategory = category;
		if (info != nullptr && info->mHasMin && info->mHasMax && info->mMax > info->mMin)
		{
			r.mHasRange = true;
			r.mMin = float(info->mMin);
			r.mMax = float(info->mMax);
		}
		const uint32_t index = mRecordedCount++;
		if (aggregate) { ++mAggregatedCount; }

		if (isGroup)
		{
			const uint32_t values = std::min<uint32_t>(info->mEnumValues.Size(), AggregateStore::kMaxEnumCount);
			for (uint32_t v = 0; v < values; ++v)
			{
				if (mGroupDefCount >= AggregateStore::kMaxGroups - 1)
				{
					if (!mGroupCapWarned) { DIA_LOG_WARNING("HIS", "cell history: more than %u groups; '%s=%s' and further groups are not recorded", AggregateStore::kMaxGroups - 1, desc.id.AsChar(), info->mEnumValues[v].mId.AsCStr()); }
					mGroupCapWarned = true;
					break;
				}
				char buf[StringCRC::kStringLength];
				snprintf(buf, sizeof(buf), "%s=%s", desc.id.AsChar(), info->mEnumValues[v].mId.AsCStr());
				const StringCRC gid(buf);
				if (!mAggregates.AddGroup(gid)) { continue; }
				GroupDef& g = mGroupDefs[mGroupDefCount++];
				g.mId = gid;
				g.mSourceLayer = index;
				g.mValue = v;
				g.mSize = 0;
			}
			if (mGroupDefCount > 0 && mMasks == nullptr) { mMasks = new uint8_t[size_t(AggregateStore::kMaxGroups - 1) * mCellCount]; }
			mHasGroups = mGroupDefCount > 0;
			mGroupsDirty = true;
		}

		if (aggregate && mRing != nullptr)
		{
			bool track = info != nullptr && HistoryTrack(*info);
			for (uint32_t i = 0; i < mConfig.mTrackCount && !track; ++i) { track = (StringCRC(mConfig.mTrack[i].AsCStr()) == desc.id); }
			if (track) { TrackRecorded(r); }
		}
	}

	StringCRC CellHistoryRecorder::RecordedLayerId(uint32_t i) const
	{
		for (uint32_t k = 0; k < mRecordedCount; ++k)
		{
			if (mRecorded[k].mAggregated)
			{
				if (i == 0) { return mRecorded[k].mId; }
				--i;
			}
		}
		return StringCRC();
	}

	bool CellHistoryRecorder::TrackRecorded(Recorded& r)
	{
		if (r.mTracked) { return true; }
		if (mRing == nullptr || !r.mAggregated) { return false; }
		if (!r.mCategory && !r.mHasRange)
		{
			DIA_LOG_ERROR("HIS", "cell history: cannot track float layer '%s': its schema has no min/max range", r.mId.AsChar());
			return false;
		}
		if (!mRing->Track(r.mId, r.mCategory ? CellRingKind::Category8 : CellRingKind::Float16, r.mMin, r.mMax)) { return false; }
		r.mTracked = true;
		r.mHasVersion = false;	// force a read into the private cache on the next sample
		r.mCacheValid = false;
		return true;
	}

	bool CellHistoryRecorder::TrackLayer(StringCRC layer)
	{
		const int i = FindRecorded(layer);
		return i >= 0 && TrackRecorded(mRecorded[i]);
	}

	namespace
	{
		void SetError(CellHistoryRecorder::TrackResult& out, const char* layer, const char* reason)
		{
			if (out.mError[0] != '\0') { return; }	// keep the first problem
			snprintf(out.mError, sizeof(out.mError), "layer '%s' %s", layer, reason);
		}
	}

	bool CellHistoryRecorder::Track(const StringCRC* layers, uint32_t n, TrackResult& out)
	{
		out = TrackResult();
		if (n == 0 || layers == nullptr)
		{
			snprintf(out.mError, sizeof(out.mError), "no layers given");
			DIA_LOG_ERROR("HIS", "cell history: track request rejected: no layers given");
			return false;
		}
		if (mRing == nullptr && ResolveGrid()) { ResolveLayers(); }
		if (mRing == nullptr)
		{
			snprintf(out.mError, sizeof(out.mError), "no recorded grid yet");
			DIA_LOG_ERROR("HIS", "cell history: track request rejected: no recorded grid");
			return false;
		}

		// Validate every layer before changing anything.
		bool valid = true;
		for (uint32_t i = 0; i < n; ++i)
		{
			const char* reason = nullptr;
			const CellLayerDesc* desc = mRegistry.Find(layers[i]);
			if (desc == nullptr) { reason = "does not exist"; }
			else if (desc->grid != mGrid) { reason = "is on another grid"; }
			else
			{
				const int idx = FindRecorded(layers[i]);
				if (idx < 0 || !mRecorded[idx].mAggregated) { reason = "is not a recorded layer (history.aggregate is false or it is beyond the aggregate cap)"; }
				else if (!mRecorded[idx].mCategory && !mRecorded[idx].mHasRange) { reason = "is a float layer whose schema has no min/max range"; }
			}
			if (reason != nullptr)
			{
				valid = false;
				SetError(out, layers[i].AsChar(), reason);
				DIA_LOG_ERROR("HIS", "cell history: cannot track layer '%s': it %s", layers[i].AsChar(), reason);
			}
		}
		if (!valid) { return false; }

		// Apply; roll back this call's additions if the ring refuses one.
		StringCRC added[kMaxRecordedLayers];
		uint32_t addedCount = 0;
		for (uint32_t i = 0; i < n; ++i)
		{
			Recorded& r = mRecorded[FindRecorded(layers[i])];
			if (r.mTracked) { continue; }	// idempotent (also covers duplicates in the list)
			if (!TrackRecorded(r))
			{
				SetError(out, layers[i].AsChar(), "could not be tracked (no room in the cell budget)");
				for (uint32_t k = 0; k < addedCount; ++k)
				{
					const int idx = FindRecorded(added[k]);
					mRing->Untrack(added[k]);
					mRecorded[idx].mTracked = false;
				}
				DIA_LOG_ERROR("HIS", "cell history: track request rolled back: %s", out.mError);
				return false;
			}
			added[addedCount++] = layers[i];
		}
		out.mOk = true;
		out.mChanged = addedCount;
		out.mWindowLength = mRing->WindowLength();
		DIA_LOG_INFO("HIS", "cell history: tracking %u new layer(s); per-cell window is now %u samples", addedCount, out.mWindowLength);
		return true;
	}

	bool CellHistoryRecorder::Untrack(const StringCRC* layers, uint32_t n, TrackResult& out)
	{
		out = TrackResult();
		if (n == 0 || layers == nullptr)
		{
			snprintf(out.mError, sizeof(out.mError), "no layers given");
			DIA_LOG_ERROR("HIS", "cell history: untrack request rejected: no layers given");
			return false;
		}
		bool valid = true;
		for (uint32_t i = 0; i < n; ++i)
		{
			const int idx = FindRecorded(layers[i]);
			const char* reason = nullptr;
			if (idx < 0) { reason = "is unknown"; }
			else if (!mRecorded[idx].mTracked) { reason = "is not tracked"; }
			if (reason != nullptr)
			{
				valid = false;
				SetError(out, layers[i].AsChar(), reason);
				DIA_LOG_ERROR("HIS", "cell history: cannot untrack layer '%s': it %s", layers[i].AsChar(), reason);
			}
		}
		if (!valid) { return false; }

		for (uint32_t i = 0; i < n; ++i)
		{
			Recorded& r = mRecorded[FindRecorded(layers[i])];
			if (!r.mTracked) { continue; }	// duplicate in the list
			mRing->Untrack(r.mId);
			r.mTracked = false;
			++out.mChanged;
			if (!NeedsCache(r))
			{
				delete[] r.mCache;
				r.mCache = nullptr;
				r.mCacheValid = false;
			}
		}
		out.mOk = true;
		out.mWindowLength = mRing->WindowLength();
		DIA_LOG_INFO("HIS", "cell history: untracked %u layer(s); per-cell window is now %u samples", out.mChanged, out.mWindowLength);
		return true;
	}

	void CellHistoryRecorder::BuildStatus(HistoryStatus& out) const
	{

		out = HistoryStatus();
		out.mSampleCount = mSampleCount;
		out.mRunId = mRunId;
		out.mRecordedLayers = mAggregatedCount;
		out.mSkippedLayers = mSkippedLayers;
		out.mVersionGatedSkips = mVersionGatedSkips;
		out.mAggregateWindow = mAggregates.WindowLength();
		out.mAggregateFirst = mAggregates.FirstRetained();
		out.mAggregateLatest = mAggregates.LatestSample();
		out.mAggregateRetained = mAggregates.RetainedCount();
		out.mAggregateBytes = mAggregates.BytesUsed();
		out.mAggregateBudget = mAggregates.Budget();
		out.mEventFirst = mEvents.FirstRetainedSample();
		out.mEventLatest = mEvents.LatestSample();
		out.mEventCount = mEvents.EventCount();
		out.mEventBytes = mEvents.BytesUsed();
		out.mEventBudget = mEvents.Budget();
		out.mCellBudget = mBudgets.mCellBytes;
		if (mRing != nullptr)
		{
			out.mWindowLength = mRing->WindowLength();
			out.mCellFirst = mRing->FirstRetained();
			out.mCellLatest = mRing->LatestSample();
			out.mCellRetained = mRing->RetainedCount();
			out.mCellBytes = mRing->BytesUsed();
			out.mCellGapCount = std::min<uint32_t>(mRing->GapCount(), HistoryStatus::kMaxGaps);
			for (uint32_t i = 0; i < out.mCellGapCount; ++i) { const CellRingGap g = mRing->Gap(i); out.mCellGaps[i].mFirst = g.mFirstDropped; out.mCellGaps[i].mLast = g.mLastDropped; }
		}
		out.mAggregateGapCount = std::min<uint32_t>(mAggregates.GapCount(), HistoryStatus::kMaxGaps);
		for (uint32_t i = 0; i < out.mAggregateGapCount; ++i) { const AggregateGap g = mAggregates.Gap(i); out.mAggregateGaps[i].mFirst = g.mFirstDropped; out.mAggregateGaps[i].mLast = g.mLastDropped; }
		out.mEventGapCount = std::min<uint32_t>(mEvents.GapCount(), HistoryStatus::kMaxGaps);
		for (uint32_t i = 0; i < out.mEventGapCount; ++i) { const EventGap g = mEvents.Gap(i); out.mEventGaps[i].mFirst = g.mFirstDropped; out.mEventGaps[i].mLast = g.mLastDropped; }
		for (uint32_t i = 0; i < mRecordedCount && out.mTrackedCount < HistoryStatus::kMaxTracked; ++i)
		{
			if (mRecorded[i].mTracked) { out.mTracked[out.mTrackedCount++] = mRecorded[i].mId; }
		}
	}

	// ---- sampling -------------------------------------------------------------------------------------------

	bool CellHistoryRecorder::NeedsCache(const Recorded& r) const
	{
		return r.mTracked || r.mIsGroup || mHasGroups;
	}

	const float* CellHistoryRecorder::ReadValues(Recorded& r, const CellLayerDesc& desc, uint64_t version)
	{
		float* buf = mScratch;
		if (NeedsCache(r))
		{
			if (r.mCache == nullptr) { r.mCache = new float[mCellCount]; }
			buf = r.mCache;
		}
		// Live values only: the layer's own read, never the registry's view value source.
		desc.read(buf, mCellCount);
		r.mCacheValid = (buf == r.mCache);
		r.mVersion = version;
		r.mHasVersion = true;
		r.mReadThisSample = true;
		return buf;
	}

	const float* CellHistoryRecorder::ValuesFor(Recorded& r, const CellLayerDesc& desc, uint64_t version)
	{
		if ((r.mReadThisSample || !r.mChanged) && r.mCacheValid && r.mCache != nullptr) { return r.mCache; }
		return ReadValues(r, desc, version);
	}

	void CellHistoryRecorder::RebuildMasks()
	{
		for (uint32_t gi = 0; gi < mGroupDefCount; ++gi)
		{
			GroupDef& g = mGroupDefs[gi];
			uint8_t* mask = mMasks + size_t(gi) * mCellCount;
			const Recorded& src = mRecorded[g.mSourceLayer];
			uint32_t count = 0;
			if (src.mCacheValid && src.mCache != nullptr)
			{
				const float want = float(g.mValue);
				for (uint32_t c = 0; c < mCellCount; ++c)
				{
					const uint8_t m = (src.mCache[c] == want) ? 1u : 0u;
					mask[c] = m;
					count += m;
				}
			}
			else
			{
				std::fill(mask, mask + mCellCount, uint8_t(0));
			}
			g.mSize = uint16_t(std::min<uint32_t>(count, 65535u));
		}
	}

	void CellHistoryRecorder::PushHeader(uint32_t sample, const SampleInfo& info)
	{
		const uint32_t window = mAggregates.WindowLength();
		const uint32_t limit = window > 0 ? window : kDefaultHeaderWindow;
		if (mHeaderCount == 0) { mHeaderFirst = sample; }
		while (mHeaderCount >= limit)
		{
			mHeaderHead = (mHeaderHead + 1) % mHeaderCap;
			--mHeaderCount;
			++mHeaderFirst;
		}
		if (mHeaderCount == mHeaderCap)
		{
			uint32_t newCap = mHeaderCap == 0 ? 64u : mHeaderCap * 2u;
			if (newCap > limit) { newCap = limit; }
			SampleHeader* grown = new SampleHeader[newCap];
			for (uint32_t i = 0; i < mHeaderCount; ++i) { grown[i] = mHeaders[(mHeaderHead + i) % mHeaderCap]; }
			delete[] mHeaders;
			mHeaders = grown;
			mHeaderCap = newCap;
			mHeaderHead = 0;
		}
		SampleHeader& h = mHeaders[(mHeaderHead + mHeaderCount) % mHeaderCap];
		h.mTick = info.tick;
		Dia::ValueSchema::SetText(h.mLabel, info.label);
		h.mBand = info.band;
		++mHeaderCount;
	}

	SampleResult CellHistoryRecorder::Sample(const SampleInfo& info)
	{
		DIA_TRACE_ZONE("cellhistory.sample", ::Dia::Observation::Trace::Category::kDiaGraphics);

		SampleResult result;
		const uint32_t idx = mSampleCount;
		result.mSample = idx;

		if (ResolveGrid()) { ResolveLayers(); }
		mEvents.CommitSample(idx);
		PushHeader(idx, info);
		++mSampleCount;

		if (mAggregatedCount == 0 || mGrid == nullptr)
		{
			if (!mNoLayersWarned)
			{
				DIA_LOG_WARNING("HIS", "cell history: Sample %u has no recorded layers; only the sample header is kept", idx);
				mNoLayersWarned = true;
			}
			return result;
		}

		mFrozen = true;
		const bool aggOk = mAggregates.BeginSample(idx);
		const bool ringOk = mRing != nullptr && mRing->TrackedCount() > 0 && mRing->BeginSample(idx);

		// 1. Version scan (cheap) and grouping layers first: membership feeds every other layer's rows.
		const CellLayerDesc* descs[kMaxRecordedLayers];
		uint64_t versions[kMaxRecordedLayers];
		bool groupsDirty = mGroupsDirty;
		for (uint32_t i = 0; i < mRecordedCount; ++i)
		{
			Recorded& r = mRecorded[i];
			r.mReadThisSample = false;
			r.mChanged = false;
			descs[i] = mRegistry.Find(r.mId);
			versions[i] = 0;
			if (descs[i] == nullptr) { continue; }
			versions[i] = descs[i]->version();
			r.mChanged = !r.mHasVersion || versions[i] != r.mVersion;
			if (r.mIsGroup && r.mChanged) { groupsDirty = true; }
		}
		for (uint32_t i = 0; i < mRecordedCount; ++i)
		{
			Recorded& r = mRecorded[i];
			if (r.mIsGroup && r.mChanged && descs[i] != nullptr) { ReadValues(r, *descs[i], versions[i]); }
		}
		if (groupsDirty && mHasGroups) { RebuildMasks(); }
		mGroupsDirty = false;

		if (aggOk)
		{
			mAggregates.SetGroupSize(AggregateStore::WorldGroup(), uint16_t(std::min<uint32_t>(mCellCount, 65535u)));
			for (uint32_t gi = 0; gi < mGroupDefCount; ++gi) { mAggregates.SetGroupSize(mGroupDefs[gi].mId, mGroupDefs[gi].mSize); }
		}

		// 2. Rows and per-cell slots.
		DIA_PROFILE_SCOPE("cellhistory.aggregate_pass", ::Dia::Observation::Profile::Category::kDiaGraphics);
		for (uint32_t i = 0; i < mRecordedCount; ++i)
		{
			Recorded& r = mRecorded[i];
			const CellLayerDesc* desc = descs[i];
			if (desc == nullptr)
			{
				// Unregistered since layout: its aggregate row stays unwritten; a tracked layer repeats its last cells.
				if (ringOk && r.mTracked && r.mCacheValid) { mRing->Append(r.mId, r.mCache, mCellCount); }
				continue;
			}

			bool rowDone = false;
			if (aggOk && r.mAggregated && !r.mChanged && !groupsDirty && idx > 0 && r.mHasVersion && (!NeedsCache(r) || r.mCacheValid))
			{
				if (mAggregates.CopyRowFrom(idx - 1, r.mId)) { rowDone = true; ++mVersionGatedSkips; }
			}

			if (aggOk && r.mAggregated && !rowDone)
			{
				const bool wasCached = !r.mChanged && r.mCacheValid;
				const float* values = ValuesFor(r, *desc, versions[i]);
				if (wasCached) { ++mVersionGatedSkips; }
				if (r.mCategory)
				{
					mAggregates.AccumulateCategory(r.mId, AggregateStore::WorldGroup(), values, nullptr, mCellCount);
					for (uint32_t gi = 0; gi < mGroupDefCount; ++gi) { mAggregates.AccumulateCategory(r.mId, mGroupDefs[gi].mId, values, mMasks + size_t(gi) * mCellCount, mCellCount); }
				}
				else
				{
					mAggregates.AccumulateFloat(r.mId, AggregateStore::WorldGroup(), values, nullptr, mCellCount);
					for (uint32_t gi = 0; gi < mGroupDefCount; ++gi) { mAggregates.AccumulateFloat(r.mId, mGroupDefs[gi].mId, values, mMasks + size_t(gi) * mCellCount, mCellCount); }
				}
			}
			if (r.mAggregated && (rowDone || aggOk)) { ++result.mLayersRecorded; }

			if (ringOk && r.mTracked)
			{
				DIA_PROFILE_SCOPE("cellhistory.percell_append", ::Dia::Observation::Profile::Category::kDiaGraphics);
				const float* values = ValuesFor(r, *desc, versions[i]);
				mRing->Append(r.mId, values, mCellCount);
			}
		}
		return result;
	}

	// ---- maintenance / reads --------------------------------------------------------------------------------

	void CellHistoryRecorder::ResetGate()
	{
		for (uint32_t i = 0; i < mRecordedCount; ++i)
		{
			mRecorded[i].mHasVersion = false;
			mRecorded[i].mCacheValid = false;
		}
		mGroupsDirty = true;
	}

	void CellHistoryRecorder::TruncateAfter(uint32_t sample)
	{
		mAggregates.TruncateAfter(sample);
		if (mRing != nullptr) { mRing->TruncateAfter(sample); }
		mEvents.TruncateAfter(sample);
		if (mSampleCount > sample + 1) { mSampleCount = sample + 1; }
		if (mHeaderCount > 0)
		{
			if (sample < mHeaderFirst) { mHeaderCount = 0; }
			else { mHeaderCount = std::min(mHeaderCount, sample + 1 - mHeaderFirst); }
		}
		ResetGate();
	}

	void CellHistoryRecorder::ResetHistory(uint64_t runIdSeed)
	{
		mAggregates.Clear();
		if (mRing != nullptr) { mRing->Clear(); }
		mEvents.Clear();
		mSampleCount = 0;
		mHeaderCount = 0;
		mHeaderHead = 0;
		mHeaderFirst = 0;
		ResetGate();
		uint64_t id = SplitMix64(runIdSeed);
		if (id == mRunId) { id = SplitMix64(id + 1); }
		mRunId = id;
	}

	const Dia::Graphics::CellGridDesc* CellHistoryRecorder::ActiveGrid()
	{
		ResolveGrid();
		return mGrid;
	}

	bool CellHistoryRecorder::HasHeader(uint32_t sample) const
	{
		return mHeaderCount > 0 && sample >= mHeaderFirst && (sample - mHeaderFirst) < mHeaderCount;
	}

	const SampleHeader& CellHistoryRecorder::Header(uint32_t sample) const
	{
		if (!HasHeader(sample)) { return sEmptyHeader; }
		return mHeaders[(mHeaderHead + (sample - mHeaderFirst)) % mHeaderCap];
	}

	bool CellHistoryRecorder::Aggregate(uint32_t sample, StringCRC layer, StringCRC group, AggregateStats& out) const
	{
		return mAggregates.Aggregate(sample, layer, group, out);
	}

	bool CellHistoryRecorder::CategoryCounts(uint32_t sample, StringCRC layer, StringCRC group, uint16_t* out, uint32_t n) const
	{
		return mAggregates.CategoryCounts(sample, layer, group, out, n);
	}

	bool CellHistoryRecorder::CellValues(uint32_t sample, StringCRC layer, float* out, uint32_t n) const
	{
		return mRing != nullptr && mRing->Read(sample, layer, out, n);
	}

	EventRange CellHistoryRecorder::Events(uint32_t fromSample, uint32_t toSample) const
	{
		EventRange r;
		r.mLog = &mEvents;
		r.mFrom = fromSample;
		r.mTo = toSample;
		return r;
	}
}
