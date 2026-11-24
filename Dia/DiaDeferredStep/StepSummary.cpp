#include <DiaDeferredStep/StepSummary.h>

namespace Dia::DeferredStep
{
	namespace
	{
		uint64_t Sub(uint64_t a, uint64_t b) { return a > b ? a - b : 0; }
		uint16_t Clamp16(uint32_t v) { return v > 0xFFFFu ? static_cast<uint16_t>(0xFFFFu) : static_cast<uint16_t>(v); }
	}

	Dia::Observation::Profile::StepSummaryRecord BuildStepSummary(
		const LaunchInfo& info,
		const RangeTiming* timings,
		uint32_t count,
		const StepTimes& times,
		const StepSummaryInputs& inputs)
	{
		namespace P = Dia::Observation::Profile;
		P::StepSummaryRecord r;
		r.owner = info.channelId.Value();
		r.kickKeyLow = static_cast<uint32_t>(info.kickKey & 0xFFFFFFFFu);   // LOSSY, matches JobTag
		r.applyKey = inputs.applyKey;
		r.kickNs = times.kickNs;
		r.readyNs = times.readyNs;
		r.applyNs = times.applyNs;
		r.stallNs = times.stallNs;

		if (inputs.restoredFromSave)
		{
			r.stallNs = 0;
			r.flags = P::StepSummaryFlags::kRestoredFromSave;
			return r;
		}

		if (inputs.ranInlineWholly)
		{
			r.rangeCount = 1;
			r.cpuNs = Sub(times.readyNs, times.kickNs);
			r.firstRangeStartNs = times.kickNs;
			r.flags |= P::StepSummaryFlags::kRanInlineWholly;
		}
		else if (timings != nullptr)
		{
			uint32_t fallbacks = 0;
			uint32_t stolen = 0;
			bool haveStart = false;
			for (uint32_t i = 0; i < count; ++i)
			{
				const RangeTiming& t = timings[i];
				r.cpuNs += Sub(t.endNs, t.startNs);
				r.capWaitNs += t.capWaitNs;
				if (t.flags & kRangeStolen)
				{
					++stolen;
				}
				else if (t.flags & kRangeTrySubmitFailed)
				{
					++fallbacks;
				}
				else if (t.admitNs != 0)
				{
					r.workerWaitNs += Sub(t.startNs, t.admitNs);
				}
				if (t.startNs != 0 && (!haveStart || t.startNs < r.firstRangeStartNs))
				{
					r.firstRangeStartNs = t.startNs;
					haveStart = true;
				}
			}
			r.rangeCount = Clamp16(count);
			r.inlineFallbacks = Clamp16(fallbacks);
			r.stolenRanges = Clamp16(stolen);
		}

		if (r.stallNs > 0)
		{
			r.flags |= P::StepSummaryFlags::kStalled;
		}
		return r;
	}
}
