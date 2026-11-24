#pragma once
#ifdef DIA_DEBUG

// HistoryTopics: the live topics of DiaCellHistory Feature 5 (Export). The owner (a stage / runner on the recorder's PU, SimPU)
// calls Tick() once per sim frame. Messages go out through ITopicPublisher, which the application implements over
// DiaDebugServer's ordered, any-thread PublishTopicOrdered (latest-wins would lose sample rows). The recorder and the publisher are
// borrowed and must outlive this object. No locks: Tick and the recorder share one PU.
//
//   cells.history.aggregates  { structure, fromSample, count, samples, groupSizes, rows, structureChanged? }
//   cells.history.events      { events:[file-shaped] }
//
// AGGREGATES. At most one burst per MinAggregateInterval (default 1 s, injectable clock), only when new samples exist AND a
// subscriber exists. The payload covers ONLY the new samples [fromSample, fromSample+count), with the file's field names and
// encodings (shared row builders, HistoryRows). A backlog (a long pause) is split into several messages of consecutive sample
// ranges, each at most MaxMessageBytes() (default 512 KB; the transport cap is 1 MB), at most kMaxAggregateMessagesPerTick per
// Tick (the rest follows on the next interval). When StructureHash() changes while subscribed, the next message carries the new
// `structure` and `structureChanged:true`: a client re-fetches cells.history.meta. Topics never replay history: the first message
// after a subscriber appears starts at the recorder's SampleCount at that moment; catch-up is the queries' job.
// EVENTS. Every Tick, each committed event exactly once, batched at MaxEventsPerMessage() (default 256) per message, at most
// kMaxEventMessagesPerTick messages per Tick (the rest follows next Tick). The cursor is (sample, index within sample), so a batch
// may end mid-sample. Events become visible when the recorder commits their sample (Sample()), so "per frame" means per Tick.
// NO SUBSCRIBER. Nothing is built; both cursors advance to the recorder's head so a late subscriber gets no unbounded backlog.
// RESET. If the recorder was truncated or reset (SampleCount shrank below a cursor, or RunId changed) both cursors move to the new
// head: events are neither replayed nor skipped from there. (A truncate that is followed by re-sampling back to the old head
// between two Ticks cannot be seen; restores are command-driven and the client sees `fromSample`/`structure` anyway.)
// DROPS. If Publish returns false the message is lost (the transport FIFO is drop-newest): the cursor still advances, the drop is
// counted and ONE Warning per second is logged; a client notices the gap through `fromSample` (aggregates) and re-fetches by query.
//
// Logs (HIS): Info when the first subscriber is seen and when the last one leaves; Warning on drops. Metrics: TopicObservation.
// Profile: cellhistory.topic_build.

#include <DiaCore/CRC/StringCRC.h>

#include <stdint.h>

namespace Json { class Value; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class TopicObservation;

	class ITopicPublisher
	{
	public:
		virtual ~ITopicPublisher() = default;
		// Queues one message. false when the transport rejected it (queue full). Callable on the recorder's PU.
		virtual bool Publish(Dia::Core::StringCRC topic, const Json::Value& payload) = 0;
		// Current subscriber count across the topics (0 = nobody listens; Tick then does no work).
		virtual uint32_t SubscriberCount() const = 0;
	};

	class HistoryTopics
	{
	public:
		using ClockFn = double(*)(void* user);	// seconds, monotonic

		static constexpr uint32_t kDefaultMaxEventsPerMessage = 256;
		static constexpr uint32_t kMaxEventMessagesPerTick = 8;
		static constexpr uint32_t kMaxAggregateMessagesPerTick = 8;
		static constexpr uint32_t kDefaultMaxMessageBytes = 512u * 1024u;
		static constexpr uint32_t kMinMaxMessageBytes = 256;
		static constexpr uint32_t kHardMaxMessageBytes = 1024u * 1024u;

		HistoryTopics(const CellHistoryRecorder& recorder, ITopicPublisher& publisher);
		HistoryTopics(const HistoryTopics&) = delete;
		HistoryTopics& operator=(const HistoryTopics&) = delete;

		void Tick();

		void SetClock(ClockFn fn, void* user) { mClock = fn; mClockUser = user; }
		void SetObservation(TopicObservation* obs) { mObs = obs; }
		void SetMaxEventsPerMessage(uint32_t n);						// clamped to [1, 256]
		void SetMinAggregateInterval(double seconds) { mInterval = seconds < 0.0 ? 0.0 : seconds; }
		void SetMaxMessageBytes(uint32_t bytes);						// test seam; clamped to [kMinMaxMessageBytes, kHardMaxMessageBytes]

		uint32_t MaxEventsPerMessage() const { return mMaxEvents; }
		double   MinAggregateInterval() const { return mInterval; }
		uint32_t MaxMessageBytes() const { return mMaxBytes; }
		uint64_t AggregateMessages() const { return mAggMessages; }
		uint64_t EventMessages() const { return mEventMessages; }
		uint64_t Drops() const { return mDrops; }

	private:
		double Now() const;
		bool Send(Dia::Core::StringCRC topic, const Json::Value& payload);
		void PublishAggregates(uint32_t count, double now);
		void PublishEvents(uint32_t count);

		const CellHistoryRecorder&	mRecorder;
		ITopicPublisher&			mPublisher;
		TopicObservation*			mObs = nullptr;
		ClockFn						mClock = nullptr;
		void*						mClockUser = nullptr;
		uint32_t					mMaxEvents = kDefaultMaxEventsPerMessage;
		uint32_t					mMaxBytes = kDefaultMaxMessageBytes;
		double						mInterval = 1.0;

		bool						mInitialised = false;
		bool						mHadSubscribers = false;
		uint64_t					mRunId = 0;
		uint32_t					mAggEnd = 0;			// first sample not yet published as aggregates
		uint32_t					mEventSample = 0;		// events cursor: sample ...
		uint32_t					mEventSkip = 0;			// ... and number of that sample's events already delivered
		uint64_t					mStructure = 0;
		bool						mStructureChanged = false;
		double						mLastAggregateTime = 0.0;
		bool						mPublishedAggregates = false;
		double						mLastWarnTime = 0.0;
		bool						mWarned = false;
		uint64_t					mAggMessages = 0;
		uint64_t					mEventMessages = 0;
		uint64_t					mDrops = 0;
	};
}

#endif // DIA_DEBUG
