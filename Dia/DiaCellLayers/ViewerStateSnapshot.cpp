#ifdef DIA_DEBUG
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaObservation/Log/DiaLog.h>

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
	}

	void ViewerStateSnapshotPublisher::Arm(uint64_t frame)
	{
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
			Json::Value marker(Json::objectValue);
			marker["error"] = "snapshot too large";
			marker["simFrame"] = static_cast<Json::UInt64>(frame);
			text = WriteCompact(marker);
		}
		mSink.Write(text.data(), static_cast<uint32_t>(text.size()));
		++mPublishCount;
	}

	ViewerStateAdapter::ViewerStateAdapter(const Dia::VisualDebugger::IDebugDomain& identity, IViewerStateSource& source,
		IViewerStateDemand& demand, CommandFn onCommand)
		: mId(identity.GetDomainId()), mName(identity.GetDisplayName()), mDescription(identity.GetDescription()),
		  mGroup(identity.GetGroup()), mAccent(identity.GetAccentColour()), mSource(source), mDemand(demand), mOnCommand(onCommand)
	{
	}

	void ViewerStateAdapter::GetJSONState(Json::Value& out)
	{
		mDemand.RequestState();
		std::string text;
		if (!mSource.ReadLatest(text))
		{
			WritePlaceholder(out, "pending", Json::Value(true));
			return;
		}
		Json::Value parsed;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader->parse(text.data(), text.data() + text.size(), &parsed, &errs) || !parsed.isObject())
		{
			WritePlaceholder(out, "error", Json::Value("unreadable snapshot"));
			return;
		}
		out = parsed;
		if (!out.isMember("drawers")) { out["drawers"] = Json::Value(Json::arrayValue); }	// the too-large marker has no panel fields
		if (!out.isMember("stats")) { out["stats"] = Json::Value(Json::objectValue); }
	}

	void ViewerStateAdapter::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
	{
		if (mOnCommand) { mOnCommand(cmd, args); }
	}
}

#endif // DIA_DEBUG
