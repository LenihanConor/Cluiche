#ifdef DIA_DEBUG

#include <DiaCellHistory/HistoryTopics.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryRows.h>
#include <DiaCellHistory/TopicObservation.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <json/json.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;

	namespace
	{
		const char* const kAggTopicName = "cells.history.aggregates";
		const char* const kEventsTopicName = "cells.history.events";

		size_t JsonSize(const Json::Value& v)
		{
			Json::StreamWriterBuilder b;
			b["indentation"] = "";
			return Json::writeString(b, v).size();
		}

		// Largest n in [1, maxN] with sizeOf(n) <= budget; 1 when even a single item does not fit. sizeOf must be monotonic.
		template <class SizeFn>
		uint32_t FitCount(uint32_t maxN, size_t budget, SizeFn sizeOf)
		{
			if (maxN <= 1 || sizeOf(1) > budget) { return 1; }
			if (sizeOf(maxN) <= budget) { return maxN; }
			uint32_t lo = 1, hi = 2;
			while (hi < maxN && sizeOf(hi) <= budget) { lo = hi; hi = (hi > maxN / 2) ? maxN : hi * 2; }
			while (hi - lo > 1)
			{
				const uint32_t mid = lo + (hi - lo) / 2;
				if (sizeOf(mid) <= budget) { lo = mid; } else { hi = mid; }
			}
			return lo;
		}

		struct EventScan
		{
			uint32_t					startSample = 0;
			uint32_t					startSkip = 0;
			const Rows::EventContext*	ctx = nullptr;
			std::vector<Json::Value>*	out = nullptr;
			size_t						limit = 0;
			uint32_t					cur = 0xFFFFFFFFu;
			uint32_t					index = 0;
			bool						full = false;
			uint32_t					nextSample = 0;
			uint32_t					nextSkip = 0;
		};

		void ScanEvent(const EventView& e, void* context)
		{
			EventScan& c = *static_cast<EventScan*>(context);
			if (e.mSample != c.cur) { c.cur = e.mSample; c.index = 0; }
			const uint32_t myIndex = c.index++;
			if (e.mSample == c.startSample && myIndex < c.startSkip) { return; }
			if (c.full) { return; }
			if (c.out->size() >= c.limit)
			{
				c.full = true;
				c.nextSample = e.mSample;
				c.nextSkip = myIndex;
				return;
			}
			c.out->push_back(Rows::EventToJson(e, *c.ctx));
		}
	}

	HistoryTopics::HistoryTopics(const CellHistoryRecorder& recorder, ITopicPublisher& publisher)
		: mRecorder(recorder), mPublisher(publisher)
	{
		// Cursors start at the head: topics never replay history (catch-up is the queries' job).
		mRunId = recorder.RunId();
		mAggEnd = recorder.SampleCount();
		mEventSample = mAggEnd;
		mStructure = recorder.StructureHash();
	}

	void HistoryTopics::SetMaxEventsPerMessage(uint32_t n) { mMaxEvents = std::min(std::max(n, 1u), kDefaultMaxEventsPerMessage); }
	void HistoryTopics::SetMaxMessageBytes(uint32_t bytes) { mMaxBytes = std::min(std::max(bytes, kMinMaxMessageBytes), kHardMaxMessageBytes); }

	double HistoryTopics::Now() const
	{
		if (mClock != nullptr) { return mClock(mClockUser); }
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}

	bool HistoryTopics::Send(StringCRC topic, const Json::Value& payload)
	{
		const size_t bytes = JsonSize(payload);
		if (mPublisher.Publish(topic, payload))
		{
			if (mObs != nullptr) { mObs->OnMessage(bytes); }
			return true;
		}
		++mDrops;
		if (mObs != nullptr) { mObs->OnDrop(); }
		const double now = Now();
		if (!mWarned || now - mLastWarnTime >= 1.0)
		{
			mWarned = true;
			mLastWarnTime = now;
			DIA_LOG_WARNING("HIS", "HistoryTopics: publisher rejected a %s message (%zu bytes, transport queue full); a client detects the gap through fromSample and re-fetches by query (further drops within 1 s are not logged)",
				topic.AsChar(), bytes);
		}
		return false;
	}

	void HistoryTopics::Tick()
	{
		const uint32_t subs = mPublisher.SubscriberCount();
		if (subs > 0 && !mHadSubscribers) { DIA_LOG_INFO("HIS", "HistoryTopics: first subscriber (%u); publishing cells.history topics", subs); }
		else if (subs == 0 && mHadSubscribers) { DIA_LOG_INFO("HIS", "HistoryTopics: last subscriber left; topics idle"); }
		mHadSubscribers = subs > 0;

		const uint32_t count = mRecorder.SampleCount();
		const uint64_t run = mRecorder.RunId();
		if (run != mRunId || count < mAggEnd || count < mEventSample)
		{
			// Truncated or reset: continue from the new head (no replay, no skip from here on).
			mRunId = run;
			mAggEnd = count;
			mEventSample = count;
			mEventSkip = 0;
		}
		const uint64_t hash = mRecorder.StructureHash();
		if (subs == 0)
		{
			// Nobody listens: no work, but the cursors follow the head so a late subscriber gets no unbounded backlog.
			mAggEnd = count;
			mEventSample = count;
			mEventSkip = 0;
			mStructure = hash;
			mStructureChanged = false;
			return;
		}
		if (hash != mStructure) { mStructure = hash; mStructureChanged = true; }

		PublishEvents(count);
		PublishAggregates(count, Now());
	}

	void HistoryTopics::PublishAggregates(uint32_t count, double now)
	{
		if (count <= mAggEnd) { return; }
		if (mPublishedAggregates && now - mLastAggregateTime < mInterval) { return; }
		DIA_PROFILE_SCOPE("cellhistory.topic_build", ::Dia::Observation::Profile::Category::kDiaGraphics);

		const AggregateStore& agg = mRecorder.Aggregates();
		const uint32_t aggFirst = agg.Empty() ? count : agg.FirstRetained();
		uint32_t from = std::max(mAggEnd, aggFirst);
		if (from >= count) { mAggEnd = count; return; }

		std::vector<Rows::AggLayer> layers;
		Rows::CollectAggLayers(mRecorder, nullptr, aggFirst, count, layers);
		char hex[24];
		snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)mStructure);

		bool flag = mStructureChanged;
		auto build = [&](uint32_t start, uint32_t n, bool fill) -> Json::Value
		{
			Json::Value r = Rows::BuildAggregateBody(mRecorder, layers, start, n, fill);
			r["structure"] = hex;
			r["fromSample"] = start;
			r["count"] = n;
			if (flag) { r["structureChanged"] = true; }
			return r;
		};

		for (uint32_t m = 0; m < kMaxAggregateMessagesPerTick && from < count; ++m)
		{
			const uint32_t start = from;
			const uint32_t n = FitCount(count - start, mMaxBytes, [&](uint32_t k) { return JsonSize(build(start, k, false)); });
			const Json::Value payload = build(start, n, true);
			flag = false;
			mStructureChanged = false;
			if (Send(StringCRC(kAggTopicName), payload)) { ++mAggMessages; }
			from = start + n;
			mAggEnd = from;
		}
		mLastAggregateTime = now;
		mPublishedAggregates = true;
	}

	void HistoryTopics::PublishEvents(uint32_t count)
	{
		if (mEventSample >= count) { return; }
		DIA_PROFILE_SCOPE("cellhistory.topic_build", ::Dia::Observation::Profile::Category::kDiaGraphics);

		Rows::EventContext ctx;
		ctx.Collect(mRecorder);
		std::vector<Json::Value> items;
		EventScan scan;
		scan.startSample = mEventSample;
		scan.startSkip = mEventSkip;
		scan.ctx = &ctx;
		scan.out = &items;
		scan.limit = size_t(mMaxEvents) * kMaxEventMessagesPerTick;
		mRecorder.Events(mEventSample, count - 1).ForEach(&ScanEvent, &scan);
		if (scan.full) { mEventSample = scan.nextSample; mEventSkip = scan.nextSkip; }
		else { mEventSample = count; mEventSkip = 0; }

		for (size_t i = 0; i < items.size(); i += mMaxEvents)
		{
			Json::Value msg(Json::objectValue);
			Json::Value events(Json::arrayValue);
			const size_t end = std::min(items.size(), i + size_t(mMaxEvents));
			for (size_t k = i; k < end; ++k) { events.append(std::move(items[k])); }
			msg["events"] = std::move(events);
			if (Send(StringCRC(kEventsTopicName), msg)) { ++mEventMessages; }
		}
	}
}

#endif // DIA_DEBUG
