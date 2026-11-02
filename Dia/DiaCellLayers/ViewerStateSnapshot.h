#pragma once
#ifdef DIA_DEBUG

// Viewer state snapshot (Streams Migration, Task 4): how `debug.domain_state` gets the viewer's panel state without touching
// SimPU state from the Main thread. PU boundaries are crossed by streams only.
//
//  SimPU: ViewerStateSnapshotPublisher::Update(frame) runs the viewer's GetJSONState, adds "simFrame", serialises with the compact
//         writer and writes the text to an IViewerStateSink (the host backs it with a FrameStream of ViewerStateSlot). It does this
//         only while ARMED: GetJSONState costs 3 to 4 ms in Debug, so it is demand-driven. Arm(frame) is called by the host when a
//         demand event arrives; publishing continues for `armFrames` Sim frames after the last Arm (default 60 = 2 s at 30 Hz).
//  Main:  ViewerStateAdapter is the IDebugDomain registered with the DebugDomain registry instead of the live viewer. Its
//         GetJSONState reads the latest snapshot from an IViewerStateSource (a FrameStream reader), signals demand through
//         IViewerStateDemand on every call, and never touches the live viewer. Before the first snapshot it returns
//         {"pending":true,"empty":true,"drawers":[],"stats":{}}; a snapshot older than the arming window is returned as is (its
//         "simFrame" says how old). OnCommand forwards to a delegate bound to the live viewer; the registry dispatches it on SimPU.
//
// Snapshot JSON = the viewer's GetJSONState output plus a top-level "simFrame". If the compact JSON exceeds the slot capacity the
// snapshot becomes {"error":"snapshot too large","simFrame":N} and a Warning is logged once (channel CEL).
// Generic: no game types (CL-005), no singletons (CL-006). The streams themselves are wired by the application.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <cstdint>
#include <string>

namespace Dia::CellLayers
{
	// FrameStream payload: fixed 32 KB slot (Task 1 measured up to 23.8 KB compact at the Large grid with 20 layers).
	struct ViewerStateSlot
	{
		static constexpr uint32_t kCapacity = 32u * 1024u;
		uint32_t	length = 0;
		char		json[kCapacity] = {};
	};

	// Main -> Sim demand event payload (EventStream): "the viewer state was requested".
	struct ViewerStateDemandEvent {};

	// Write side (SimPU).
	class IViewerStateSink
	{
	public:
		virtual ~IViewerStateSink() = default;
		virtual void Write(const char* json, uint32_t length) = 0;
	};

	// Read side (Main).
	class IViewerStateSource
	{
	public:
		virtual ~IViewerStateSource() = default;
		// False if no snapshot has been published yet. Otherwise copies the latest snapshot JSON text into json.
		virtual bool ReadLatest(std::string& json) = 0;
	};

	// Demand signal (Main). Cheap: called on every GetJSONState.
	class IViewerStateDemand
	{
	public:
		virtual ~IViewerStateDemand() = default;
		virtual void RequestState() = 0;
	};

	class ViewerStateSnapshotPublisher
	{
	public:
		static constexpr uint64_t	kDefaultArmFrames = 60;

		// domain and sink are borrowed. capacity is the largest JSON text the sink accepts (default: the slot).
		ViewerStateSnapshotPublisher(Dia::VisualDebugger::IDebugDomain& domain, IViewerStateSink& sink,
			uint64_t armFrames = kDefaultArmFrames, uint32_t capacity = ViewerStateSlot::kCapacity);
		ViewerStateSnapshotPublisher(const ViewerStateSnapshotPublisher&) = delete;
		ViewerStateSnapshotPublisher& operator=(const ViewerStateSnapshotPublisher&) = delete;

		// Demand seen at Sim frame `frame`: publish for the next armFrames frames.
		void		Arm(uint64_t frame);
		// Once per Sim frame (after the viewer's Update). Publishes only while armed.
		void		Update(uint64_t frame);

		uint32_t	PublishCount() const { return mPublishCount; }	// snapshots written (including the too-large marker)
		bool		IsArmed() const { return mArmed; }

	private:
		Dia::VisualDebugger::IDebugDomain&	mDomain;
		IViewerStateSink&		mSink;
		uint64_t				mArmFrames;
		uint32_t				mCapacity;
		bool					mArmed = false;
		uint64_t				mArmedFrame = 0;
		bool					mWarnedTooLarge = false;
		uint32_t				mPublishCount = 0;
	};

	class ViewerStateAdapter : public Dia::VisualDebugger::IDebugDomain
	{
	public:
		typedef fastdelegate::FastDelegate2<Dia::Core::StringCRC, const Json::Value&> CommandFn;

		// Identity (id, name, description, group, accent) is copied from `identity` here; it is never consulted again, so the adapter
		// keeps no pointer to the live viewer. source and demand are borrowed. onCommand runs on whichever thread the registry
		// dispatches OnCommand on (SimPU) and is typically bound to the live viewer's OnCommand; empty = commands ignored.
		ViewerStateAdapter(const Dia::VisualDebugger::IDebugDomain& identity, IViewerStateSource& source, IViewerStateDemand& demand,
			CommandFn onCommand = CommandFn());

		Dia::Core::StringCRC GetDomainId()     const override { return mId; }
		const char*          GetDisplayName()  const override { return mName.c_str(); }
		const char*          GetDescription()  const override { return mDescription.c_str(); }
		Dia::Core::StringCRC GetGroup()        const override { return mGroup; }
		Dia::Core::RGBA      GetAccentColour() const override { return mAccent; }
		bool                 HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value& out) override;
		void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

	private:
		Dia::Core::StringCRC	mId;
		std::string				mName;
		std::string				mDescription;
		Dia::Core::StringCRC	mGroup;
		Dia::Core::RGBA			mAccent;
		IViewerStateSource&		mSource;
		IViewerStateDemand&		mDemand;
		CommandFn				mOnCommand;
	};
}

#endif // DIA_DEBUG
