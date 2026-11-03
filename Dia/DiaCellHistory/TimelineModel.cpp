#include "TimelineModel.h"

#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <chrono>
#include <cmath>

namespace Dia::CellHistory
{
	namespace
	{
		// Bucket b of n over count samples (n <= count): first sample index.
		uint32_t BucketFirst(uint32_t b, uint32_t n, uint32_t count)
		{
			return uint32_t((uint64_t(b) * count + n - 1) / n);
		}
		uint32_t BucketOf(uint32_t sample, uint32_t n, uint32_t count)
		{
			return uint32_t(uint64_t(sample) * n / count);
		}

		struct TypeInfo { Dia::Core::CRC mType; uint32_t mColour = 0; };

		struct EventTally
		{
			static constexpr uint32_t kMaxTypes = 64;
			static constexpr uint32_t kNone = 0xFFFFFFFFu;
			const Dia::ValueSchema::ValueSchemaService* mSchema = nullptr;
			uint32_t mBucketCount = 0;
			uint32_t mSampleCount = 0;
			TimelineArray<TimelineBucket>* mBuckets = nullptr;
			TypeInfo mTypes[kMaxTypes];
			uint32_t mTypeCount = 0;
			// Counts for the bucket being accumulated, by type-table slot (the last slot collects overflow types).
			uint32_t mCounts[kMaxTypes + 1] = {};
			uint32_t mFirstSeen[kMaxTypes + 1] = {};
			uint32_t mCurrent = kNone;
			uint32_t mCurrentTotal = 0;
			uint32_t mSeenSeq = 0;

			uint32_t ColourFor(Dia::Core::CRC type) const
			{
				if (mSchema != nullptr)
				{
					// FindEvent takes a StringCRC (needs the key text); the log only keeps the hash, so match by hash.
					for (unsigned int i = 0; i < mSchema->Count(); ++i)
					{
						const Dia::ValueSchema::ValueInfo* info = mSchema->At(i);
						if (info == nullptr || info->mKind != Dia::ValueSchema::ValueKind::Event) { continue; }
						if (Dia::Core::StringCRC(info->mKey.AsCStr()).Value() != type.Value()) { continue; }
						const Json::Value& c = info->mUi["colour"];
						uint32_t rgba = 0;
						if (c.isString() && Dia::ValueSchema::ParseHexColour(c.asCString(), rgba)) { return rgba; }
						break;
					}
				}
				return TimelineModel::kDefaultEventColour;
			}

			uint32_t SlotFor(Dia::Core::CRC type)
			{
				for (uint32_t i = 0; i < mTypeCount; ++i) { if (mTypes[i].mType == type) { return i; } }
				if (mTypeCount >= kMaxTypes) { return kMaxTypes; }
				mTypes[mTypeCount].mType = type;
				mTypes[mTypeCount].mColour = ColourFor(type);
				return mTypeCount++;
			}

			void Flush()
			{
				if (mCurrent == kNone) { return; }
				if (mCurrentTotal > 0)
				{
					TimelineBucket& b = (*mBuckets)[mCurrent];
					b.mEventCount = mCurrentTotal;
					bool used[kMaxTypes + 1] = {};
					for (uint32_t k = 0; k < TimelineBucket::kMaxTypesPerBucket; ++k)
					{
						int best = -1;
						for (uint32_t s = 0; s <= kMaxTypes; ++s)
						{
							if (used[s] || mCounts[s] == 0) { continue; }
							if (best < 0 || mCounts[s] > mCounts[best] || (mCounts[s] == mCounts[best] && mFirstSeen[s] < mFirstSeen[best])) { best = int(s); }
						}
						if (best < 0) { break; }
						used[best] = true;
						TimelineTypeCount& t = b.mTypes[b.mTypeCount++];
						t.mCount = mCounts[best];
						if (uint32_t(best) < kMaxTypes) { t.mType = mTypes[best].mType; t.mColour = mTypes[best].mColour; }
						else { t.mColour = TimelineModel::kDefaultEventColour; }
					}
					b.mColour = b.mTypeCount > 0 ? b.mTypes[0].mColour : TimelineModel::kDefaultEventColour;
				}
				for (uint32_t s = 0; s <= kMaxTypes; ++s) { mCounts[s] = 0; mFirstSeen[s] = 0; }
				mCurrentTotal = 0;
				mSeenSeq = 0;
				mCurrent = kNone;
			}

			void Add(const EventView& e)
			{
				if (e.mSample >= mSampleCount) { return; }
				const uint32_t bucket = BucketOf(e.mSample, mBucketCount, mSampleCount);
				if (bucket != mCurrent) { Flush(); mCurrent = bucket; }
				const uint32_t slot = SlotFor(e.mType);
				if (mCounts[slot] == 0) { mFirstSeen[slot] = mSeenSeq++; }
				++mCounts[slot];
				++mCurrentTotal;
			}
		};

		void EventVisitorFn(const EventView& e, void* ctx) { static_cast<EventTally*>(ctx)->Add(e); }
	}

	bool TimelineModel::NeedsRebuild(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC baseLayer) const
	{
		return !mBuilt || mStampSamples != recorder.SampleCount() || mStampEpoch != view.Epoch() || !(mStampLayer == baseLayer);
	}

	void TimelineModel::Build(const CellHistoryRecorder& recorder, const HistoryViewState& view, Dia::Core::StringCRC baseLayer,
		const Dia::ValueSchema::ValueSchemaService* schema, const HistoryConfig* config, uint32_t bucketCount)
	{
		const auto t0 = std::chrono::steady_clock::now();

		const uint32_t count = recorder.SampleCount();
		mRange = TimelineRange();
		mRange.mCount = count;
		mRange.mLatestSample = count > 0 ? count - 1 : 0;
		mPosition.mMode = view.Mode();
		mPosition.mSample = view.IsLive() ? mRange.mLatestSample : view.Sample();

		mBuckets.RemoveAll();
		mBandTicks.RemoveAll();
		mSpark.RemoveAll();
		mBandTicksTruncated = false;
		mPreWindow = TimelineSpan();

		CellHistoryRecorder::HistoryStatus status;
		recorder.BuildStatus(status);
		mAggregateGapCount = status.mAggregateGapCount;
		mCellGapCount = status.mCellGapCount;
		mEventGapCount = status.mEventGapCount;
		for (uint32_t i = 0; i < mAggregateGapCount; ++i) { mAggregateGaps[i] = status.mAggregateGaps[i]; }
		for (uint32_t i = 0; i < mCellGapCount; ++i) { mCellGaps[i] = status.mCellGaps[i]; }
		for (uint32_t i = 0; i < mEventGapCount; ++i) { mEventGaps[i] = status.mEventGaps[i]; }

		if (count > 0)
		{
			uint32_t n = bucketCount < 1 ? 1u : (bucketCount > kMaxBucketCount ? kMaxBucketCount : bucketCount);
			if (n > count) { n = count; }

			mBuckets.Reserve(n);
			for (uint32_t b = 0; b < n; ++b)
			{
				TimelineBucket bk;
				bk.mFirstSample = BucketFirst(b, n, count);
				bk.mLastSample = BucketFirst(b + 1, n, count) - 1;
				mBuckets.Add(bk);
			}

			{
				EventTally tally;
				tally.mSchema = schema;
				tally.mBucketCount = n;
				tally.mSampleCount = count;
				tally.mBuckets = &mBuckets;
				recorder.Events(0, count - 1).ForEach(&EventVisitorFn, &tally);
				tally.Flush();
			}

			for (uint32_t s = 0; s < count; ++s)
			{
				if (!recorder.HasHeader(s)) { continue; }
				const uint8_t band = recorder.Header(s).mBand;
				if (band == 0) { continue; }
				if (mBandTicks.Size() >= kMaxBandTicks) { mBandTicksTruncated = true; break; }
				TimelineBandTick tick;
				tick.mSample = s;
				tick.mBand = band;
				if (config != nullptr && band < config->mBandCount) { tick.mName = config->mBands[band]; }
				else { tick.mName = Dia::Core::Containers::String32("band%u", unsigned(band)); }
				mBandTicks.Add(tick);
			}

			if (status.mWindowLength > 0 && recorder.FirstRetained() > 0)
			{
				mPreWindow.mPresent = true;
				mPreWindow.mFirst = 0;
				mPreWindow.mLast = recorder.FirstRetained() - 1;
			}

			const uint32_t points = n < kMaxSparkPoints ? n : kMaxSparkPoints;
			bool any = false;
			mSpark.Reserve(points);
			for (uint32_t p = 0; p < points; ++p)
			{
				TimelineSparkPoint pt;
				pt.mFirstSample = BucketFirst(p, points, count);
				pt.mLastSample = BucketFirst(p + 1, points, count) - 1;
				double sum = 0.0;
				uint32_t valid = 0;
				for (uint32_t s = pt.mFirstSample; s <= pt.mLastSample; ++s)
				{
					AggregateStats st;
					if (recorder.Aggregate(s, baseLayer, AggregateStore::WorldGroup(), st) && !std::isnan(st.mMean))
					{
						sum += st.mMean;
						++valid;
					}
				}
				if (valid > 0) { pt.mValid = true; pt.mValue = float(sum / valid); any = true; }
				mSpark.Add(pt);
			}
			if (!any) { mSpark.RemoveAll(); }
		}

		mBuilt = true;
		mStampSamples = count;
		mStampEpoch = view.Epoch();
		mStampLayer = baseLayer;
		++mBuildCount;
		mLastBuildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}

	Json::Value TimelineModel::ToJson() const
	{
		Json::Value j(Json::objectValue);
		Json::Value range(Json::objectValue);
		range["first"] = mRange.mFirstSample;
		range["latest"] = mRange.mLatestSample;
		range["count"] = mRange.mCount;
		j["range"] = range;

		Json::Value pos(Json::objectValue);
		pos["mode"] = mPosition.mMode == ViewMode::Live ? "live" : "sample";
		pos["sample"] = mPosition.mSample;
		j["position"] = pos;
		j["liveMarker"] = HasLiveMarker() ? Json::Value(mRange.mLatestSample) : Json::Value(Json::nullValue);

		Json::Value buckets(Json::arrayValue);
		for (uint32_t i = 0; i < mBuckets.Size(); ++i)
		{
			const TimelineBucket& b = mBuckets[i];
			Json::Value o(Json::objectValue);
			o["first"] = b.mFirstSample;
			o["last"] = b.mLastSample;
			o["events"] = b.mEventCount;
			if (b.mEventCount > 0)
			{
				o["colour"] = b.mColour;
				Json::Value types(Json::arrayValue);
				for (uint32_t t = 0; t < b.mTypeCount; ++t)
				{
					Json::Value tv(Json::objectValue);
					tv["type"] = b.mTypes[t].mType.Value();
					tv["count"] = b.mTypes[t].mCount;
					tv["colour"] = b.mTypes[t].mColour;
					types.append(tv);
				}
				o["types"] = types;
			}
			buckets.append(o);
		}
		j["buckets"] = buckets;

		Json::Value ticks(Json::arrayValue);
		for (uint32_t i = 0; i < mBandTicks.Size(); ++i)
		{
			Json::Value o(Json::objectValue);
			o["sample"] = mBandTicks[i].mSample;
			o["band"] = unsigned(mBandTicks[i].mBand);
			o["name"] = mBandTicks[i].mName.AsCStr();
			ticks.append(o);
		}
		j["bandTicks"] = ticks;
		j["bandTicksTruncated"] = mBandTicksTruncated;

		auto gaps = [](const CellHistoryRecorder::StatusGap* g, uint32_t n)
		{
			Json::Value a(Json::arrayValue);
			for (uint32_t i = 0; i < n; ++i)
			{
				Json::Value o(Json::objectValue);
				o["first"] = g[i].mFirst;
				o["last"] = g[i].mLast;
				a.append(o);
			}
			return a;
		};
		Json::Value gv(Json::objectValue);
		gv["aggregate"] = gaps(mAggregateGaps, mAggregateGapCount);
		gv["cell"] = gaps(mCellGaps, mCellGapCount);
		gv["event"] = gaps(mEventGaps, mEventGapCount);
		j["gaps"] = gv;

		if (mPreWindow.mPresent)
		{
			Json::Value d(Json::objectValue);
			d["first"] = mPreWindow.mFirst;
			d["last"] = mPreWindow.mLast;
			j["preWindowDim"] = d;
		}
		else { j["preWindowDim"] = Json::Value(Json::nullValue); }

		Json::Value spark(Json::arrayValue);
		for (uint32_t i = 0; i < mSpark.Size(); ++i)
		{
			Json::Value o(Json::objectValue);
			o["first"] = mSpark[i].mFirstSample;
			o["last"] = mSpark[i].mLastSample;
			if (mSpark[i].mValid) { o["value"] = double(mSpark[i].mValue); }
			else { o["value"] = Json::Value(Json::nullValue); }
			spark.append(o);
		}
		j["sparkline"] = spark;
		return j;
	}
}
