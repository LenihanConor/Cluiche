#include <DiaDeferredStep/CappedParallelExecutor.h>

#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/RangeSplit.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia::DeferredStep
{
	namespace
	{
		uint64_t NowNs()
		{
			return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
		}
	}

	CappedParallelExecutor::CappedParallelExecutor(CapGate& gate, const LaunchInfo& info)
		: mGate(gate)
		, mInfo(info)
	{
	}

	void CappedParallelExecutor::ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn)
	{
		DIA_TRACE_ZONE("CappedParallelExecutor::ParallelFor", Dia::Observation::Trace::Category::kDiaDeferredStep);
		DIA_PROFILE_SCOPE("CappedParallelExecutor::ParallelFor", Dia::Observation::Profile::Category::kDiaDeferredStep);

		if (count == 0)
		{
			mLastRangeCount = 1;
			mTimings.assign(1, RangeTiming());
			return;
		}

		uint32_t rangeCount = ComputeRangeCount(count, minRange, mGate.GetWorkerCap(), mGate.GetRangesPerWorker());
		if (mForcedRangeCount != 0)
		{
			rangeCount = mForcedRangeCount > count ? count : mForcedRangeCount;
		}
		mLastRangeCount = rangeCount;
		// Slot [i] is written only by the thread that runs range i; nothing resizes it until all finish.
		// Range bounds come from GetRangeBounds(count, rangeCount, i): a pure function of (count, R, i),
		// so the split never depends on execution order.
		mTimings.assign(rangeCount, RangeTiming());

		std::mutex errorMutex;
		std::exception_ptr firstError;

		// The single body every range goes through, whichever thread runs it. timing.flags is preset by
		// the coordinator (before RunInline) for stolen / failed-submit ranges; otherwise it is 0.
		auto runRange = [&](uint32_t index, const GateItem* item)
		{
			DIA_TRACE_ZONE("CappedParallelExecutor::Range", Dia::Observation::Trace::Category::kDiaDeferredStep);
			DIA_PROFILE_SCOPE("CappedParallelExecutor::Range", Dia::Observation::Profile::Category::kDiaDeferredStep);

			RangeTiming& t = mTimings[index];
			const uint64_t start = NowNs();
			t.startNs = start;
			if (item == nullptr)
			{
				t.enqueueNs = start;   // range 0: run directly by the coordinator
			}
			else
			{
				t.enqueueNs = item->GetEnqueueNs();
				if (t.flags & kRangeStolen)
				{
					t.admitNs = 0;
					t.capWaitNs = start - t.enqueueNs;
					mGate.AddCapWait(t.capWaitNs);
				}
				else if (t.flags & kRangeTrySubmitFailed)
				{
					t.admitNs = 0;
					t.capWaitNs = 0;
				}
				else
				{
					t.admitNs = item->GetAdmitNs();
					t.capWaitNs = (t.admitNs > t.enqueueNs) ? (t.admitNs - t.enqueueNs) : 0;
				}
			}

			uint32_t begin = 0;
			uint32_t end = 0;
			GetRangeBounds(count, rangeCount, index, begin, end);
			try
			{
				fn(begin, end);
			}
			catch (...)
			{
				std::lock_guard<std::mutex> lock(errorMutex);
				if (!firstError) { firstError = std::current_exception(); }
			}
			t.endNs = NowNs();
		};

		// Items live for the whole call; GateItem is neither copyable nor movable.
		const uint32_t helperCount = rangeCount - 1;
		std::unique_ptr<GateItem[]> items(helperCount > 0 ? new GateItem[helperCount] : nullptr);

		// The ONLY range admission call site; each range job carries its JobTag (range i of rangeCount).
		for (uint32_t i = 1; i < rangeCount; ++i)
		{
			GateItem& item = items[i - 1];
			item.Prepare(GateItem::Kind::kRange, [&runRange, &item, i] { runRange(i, &item); });
			item.SetJobTag(MakeJobTag(mInfo, static_cast<uint16_t>(i), static_cast<uint16_t>(rangeCount)));
			mGate.Admit(item);
		}

		// The coordinator works too.
		runRange(0, nullptr);

		bool warned = false;
		auto runInlineFailed = [&](uint32_t i, GateItem& item)
		{
			if (!warned)
			{
				warned = true;
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel,
					"Range submit failed; running ranges inline (job pool unavailable?)");
			}
			mTimings[i].flags = kRangeTrySubmitFailed;
			item.RunInline();
		};

		for (uint32_t i = 1; i < rangeCount; ++i)
		{
			GateItem& item = items[i - 1];
			GateItem::State state = item.GetState();
			if (state == GateItem::State::kQueued)
			{
				// Still behind the cap: take it back and run it here, so no range can be stranded
				// behind a slot the coordinator itself occupies.
				if (mGate.TrySteal(item))
				{
					mTimings[i].flags = kRangeStolen;
					item.RunInline();
					continue;
				}
				// Admitted in the meantime; fall through to wait.
			}
			else if (state == GateItem::State::kSubmitFailed)
			{
				runInlineFailed(i, item);
				continue;
			}

			if (mGate.WaitItem(item) == GateItem::State::kSubmitFailed)
			{
				runInlineFailed(i, item);
			}
		}

		mTotalRangesRun += rangeCount;

		if (firstError)
		{
			std::rethrow_exception(firstError);
		}
	}
}
