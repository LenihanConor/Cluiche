#ifdef DIA_DEBUG
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaValueSchema/ValueInfo.h>

#include <chrono>
#include <memory>

namespace Dia::CellLayers
{
	namespace
	{
		std::string WriteCompact(const Json::Value& v)
		{
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			return Json::writeString(builder, v);
		}

		uint64_t DefaultNowMs()
		{
			return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
		}

		// What the panel needs even when there is no snapshot (IDebugDomain::GetJSONState contract: drawers + stats).
		void WritePlaceholder(Json::Value& out, const char* key, Json::Value value)
		{
			out = Json::Value(Json::objectValue);
			out[key] = value;
			out["empty"] = true;
			out["drawers"] = Json::Value(Json::arrayValue);
			out["stats"] = Json::Value(Json::objectValue);
		}
	}

	ViewerStateSnapshotPublisher::ViewerStateSnapshotPublisher(Dia::VisualDebugger::IDebugDomain& domain, IViewerStateSink& sink,
		uint64_t armFrames, uint32_t capacity)
		: mDomain(domain), mSink(sink), mArmFrames(armFrames), mCapacity(capacity)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricPublished = metrics.RegisterCounter(Dia::Core::StringCRC("dia.celllayers.snapshot.published"));
		mMetricTooLarge = metrics.RegisterCounter(Dia::Core::StringCRC("dia.celllayers.snapshot.too_large"));
		mMetricArms = metrics.RegisterCounter(Dia::Core::StringCRC("dia.celllayers.snapshot.demand_windows"));
	}

	void ViewerStateSnapshotPublisher::Arm(uint64_t frame)
	{
		if (!mArmed && mMetricArms) { mMetricArms->Inc(1); }	// a new demand window, not a re-arm of an open one
		mArmed = true;
		mArmedFrame = frame;
	}

	void ViewerStateSnapshotPublisher::Update(uint64_t frame)
	{
		if (!mArmed) { return; }
		// A frame counter that went backwards (restart) counts as inside the window.
		if (frame >= mArmedFrame && frame - mArmedFrame > mArmFrames)
		{
			mArmed = false;
			return;
		}

		DIA_PROFILE_SCOPE("celllayers.snapshot.publish", ::Dia::Observation::Profile::Category::kDiaGraphics);
		Json::Value state(Json::objectValue);
		mDomain.GetJSONState(state);
		state["simFrame"] = static_cast<Json::UInt64>(frame);
		std::string text = WriteCompact(state);
		if (text.size() > mCapacity)
		{
			if (!mWarnedTooLarge)
			{
				mWarnedTooLarge = true;
				DIA_LOG_WARNING("CEL", "ViewerStateSnapshot: snapshot of %u bytes is over the %u byte slot; publishing a 'snapshot too large' marker instead",
					static_cast<unsigned>(text.size()), mCapacity);
			}
			if (mMetricTooLarge) { mMetricTooLarge->Inc(1); }
			Json::Value marker(Json::objectValue);
			marker["error"] = "snapshot too large";
			marker["simFrame"] = static_cast<Json::UInt64>(frame);
			text = WriteCompact(marker);
		}
		mSink.Write(text.data(), static_cast<uint32_t>(text.size()));
		++mPublishCount;
		if (mMetricPublished) { mMetricPublished->Inc(1); }
	}

	ViewerStateAdapter::ViewerStateAdapter(const Dia::VisualDebugger::IDebugDomain& identity, IViewerStateSource& source,
		IViewerStateDemand& demand, CommandFn onCommand)
		: mId(identity.GetDomainId()),
		  mGroup(identity.GetGroup()), mAccent(identity.GetAccentColour()), mSource(source), mDemand(demand), mOnCommand(onCommand)
	{
		Dia::ValueSchema::SetText(mName, identity.GetDisplayName());	// truncating copy; the printf-style String constructor would format '%'
		Dia::ValueSchema::SetText(mDescription, identity.GetDescription());
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricPending = metrics.RegisterCounter(Dia::Core::StringCRC("dia.celllayers.snapshot.reads_pending"));
		mMetricUnreadable = metrics.RegisterCounter(Dia::Core::StringCRC("dia.celllayers.snapshot.reads_unreadable"));
		mMetricAge = metrics.RegisterGauge(Dia::Core::StringCRC("dia.celllayers.snapshot.age_ms"));
		mNow = &DefaultNowMs;
		mText[0] = '\0';
	}

	void ViewerStateAdapter::GetJSONState(Json::Value& out)
	{
		DIA_PROFILE_SCOPE("celllayers.snapshot.read", ::Dia::Observation::Profile::Category::kDiaGraphics);
		mDemand.RequestState();
		uint32_t length = 0;
		if (!mSource.ReadLatest(mText, ViewerStateSlot::kCapacity, length))
		{
			if (mMetricPending) { mMetricPending->Inc(1); }
			mHealth.OnPending();
			WritePlaceholder(out, "pending", Json::Value(true));
			return;
		}
		Json::Value parsed;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader->parse(mText, mText + length, &parsed, &errs) || !parsed.isObject())
		{
			if (mMetricUnreadable) { mMetricUnreadable->Inc(1); }
			mHealth.OnUnreadable();
			DIA_LOG_WARNING("CEL", "ViewerStateAdapter: latest snapshot (%u bytes) is not a JSON object", length);
			WritePlaceholder(out, "error", Json::Value("unreadable snapshot"));
			return;
		}
		// Snapshot age = how long the Sim frame number has stayed the same as seen by this adapter (a stalled Sim stops advancing it).
		const uint64_t now = mNow();
		const uint64_t simFrame = parsed.isMember("simFrame") ? static_cast<uint64_t>(parsed["simFrame"].asUInt64()) : 0;
		if (!mHaveSimFrame || simFrame != mLastSimFrame) { mHaveSimFrame = true; mLastSimFrame = simFrame; mLastSimFrameChangeMs = now; }
		const uint64_t ageMs = now - mLastSimFrameChangeMs;
		mHealth.OnSnapshot(ageMs);
		if (mMetricAge) { mMetricAge->Set(static_cast<double>(ageMs)); }
		out = parsed;
		if (!out.isMember("drawers")) { out["drawers"] = Json::Value(Json::arrayValue); }	// the too-large marker has no panel fields
		if (!out.isMember("stats")) { out["stats"] = Json::Value(Json::objectValue); }
	}

	Dia::Core::StringCRC ViewerStateHealth::GetReporterName() const
	{
		return Dia::Core::StringCRC("dia.celllayers.snapshot");
	}

	Dia::Observation::Health::Health ViewerStateHealth::Report() const
	{
		Dia::Observation::Health::Health h{};
		h.errors = mUnreadable.load(std::memory_order_relaxed);
		h.status = Dia::Observation::Health::HealthStatus::kOK;
		h.reason = Dia::Core::StringCRC::kZero;
		if (mConsecutivePending.load(std::memory_order_relaxed) >= kPendingLimit)
		{
			h.status = Dia::Observation::Health::HealthStatus::kDegraded;
			h.reason = Dia::Core::StringCRC("celllayers.snapshot_never_published");
		}
		else if (mAgeMs.load(std::memory_order_relaxed) > kStaleMs)
		{
			h.status = Dia::Observation::Health::HealthStatus::kDegraded;
			h.reason = Dia::Core::StringCRC("celllayers.snapshot_stale");
		}
		return h;
	}

	void ViewerStateAdapter::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
	{
		if (mOnCommand) { mOnCommand(cmd, args); }
	}
}

#endif // DIA_DEBUG
