#include <DiaDeferredStep/DeferredStepMetrics.h>

#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia::DeferredStep
{
	namespace
	{
		using Dia::Observation::Metric::MetricInfo;

		// Info strings are stored by pointer in the registry: literals only.
		const MetricInfo kInFlightInfo        { "In flight",         "Channels not idle, refreshed each Tick",                 "channels" };
		const MetricInfo kKicksInfo           { "Kicks",             "Accepted kicks",                                          "count" };
		const MetricInfo kKicksRejectedInfo   { "Kicks rejected",    "Kicks that returned anything other than kLaunched",       "count" };
		const MetricInfo kAppliesInfo         { "Applies",           "Steps applied",                                           "count" };
		const MetricInfo kStallsInfo          { "Stalls",            "Applies that had to wait for the step to finish",         "count" };
		const MetricInfo kStallMsInfo         { "Stall time",        "Total time SimPU spent stalled",                          "ms" };
		const MetricInfo kSlackInfo           { "Slack",             "Slack of the most recently applied step",                 "keys" };
		const MetricInfo kCapUtilisationInfo  { "Cap utilisation",   "Worker slots held / worker cap at Tick time",             "ratio" };
		const MetricInfo kInlineInfo          { "Inline fallbacks",  "Ranges that ran inline after a failed TrySubmit",         "count" };
	}

	void DeferredStepMetrics::Register()
	{
		auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
		using Dia::Core::StringCRC;
		mInFlight        = reg.RegisterGauge  (StringCRC("dia.deferredstep.in_flight"),        kInFlightInfo);
		mKicks           = reg.RegisterCounter(StringCRC("dia.deferredstep.kicks"),            kKicksInfo);
		mKicksRejected   = reg.RegisterCounter(StringCRC("dia.deferredstep.kicks_rejected"),   kKicksRejectedInfo);
		mApplies         = reg.RegisterCounter(StringCRC("dia.deferredstep.applies"),          kAppliesInfo);
		mStalls          = reg.RegisterCounter(StringCRC("dia.deferredstep.stalls"),           kStallsInfo);
		mStallMs         = reg.RegisterCounter(StringCRC("dia.deferredstep.stall_ms"),         kStallMsInfo);
		mSlackKeys       = reg.RegisterGauge  (StringCRC("dia.deferredstep.slack_keys"),       kSlackInfo);
		mCapUtilisation  = reg.RegisterGauge  (StringCRC("dia.deferredstep.cap_utilisation"),  kCapUtilisationInfo);
		mInlineFallbacks = reg.RegisterCounter(StringCRC("dia.deferredstep.inline_fallbacks"), kInlineInfo);

		mPubKicks = mPubKicksRejected = mPubApplies = mPubStalls = mPubStallMs = mPubInline = 0;
	}

	void DeferredStepMetrics::Unregister()
	{
		if (IsRegistered())
		{
			auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
			for (const char* name : { "dia.deferredstep.in_flight", "dia.deferredstep.kicks", "dia.deferredstep.kicks_rejected",
				"dia.deferredstep.applies", "dia.deferredstep.stalls", "dia.deferredstep.stall_ms",
				"dia.deferredstep.slack_keys", "dia.deferredstep.cap_utilisation", "dia.deferredstep.inline_fallbacks" })
			{
				reg.Unregister(Dia::Core::StringCRC(name));   // retired: absent from snapshots until the next Register()
			}
		}
		mInFlight = nullptr;
		mKicks = nullptr;
		mKicksRejected = nullptr;
		mApplies = nullptr;
		mStalls = nullptr;
		mStallMs = nullptr;
		mSlackKeys = nullptr;
		mCapUtilisation = nullptr;
		mInlineFallbacks = nullptr;
	}

	void DeferredStepMetrics::Update(const SchedulerStats& stats, const CapStats& cap, bool hasCap)
	{
		if (!IsRegistered()) { return; }

		auto advance = [](Dia::Observation::Metric::Counter* c, uint64_t current, uint64_t& published)
		{
			if (current > published) { c->Inc(current - published); published = current; }
		};

		mInFlight->Set(static_cast<double>(stats.inFlight));
		advance(mKicks,         stats.kicks,                    mPubKicks);
		advance(mKicksRejected, stats.kicksRejected,            mPubKicksRejected);
		advance(mApplies,       stats.applies,                  mPubApplies);
		advance(mStalls,        stats.stalls,                   mPubStalls);
		advance(mStallMs,       stats.totalStallMicros / 1000u, mPubStallMs);
		mSlackKeys->Set(static_cast<double>(stats.lastApplySlackKeys));

		if (hasCap)
		{
			mCapUtilisation->Set(cap.workerCap > 0 ? static_cast<double>(cap.slotsHeld) / static_cast<double>(cap.workerCap) : 0.0);
			advance(mInlineFallbacks, cap.trySubmitFailed, mPubInline);
		}
		else
		{
			mCapUtilisation->Set(0.0);
		}
	}
}
