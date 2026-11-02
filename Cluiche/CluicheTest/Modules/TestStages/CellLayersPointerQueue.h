#pragma once
#include <DiaCellLayers/SelectionInput.h>
#include <DiaCore/Json/external/json/json.h>
#ifdef DIA_DEBUG
#include <DiaCellLayers/MirrorQueries.h>
#endif

#include <functional>
#include <string>
#include <vector>

namespace CluicheTest {

// Test-only pointer injection support for CellLayersViewerTestStageModule (application code).
// ParsePointerFrames validates the JSON params of cluichetest.celllayers.pointer_frames; PointerFrameQueue is a
// fixed-capacity plain FIFO: the sim-hosted commands push and the stage's Sim update pops, both on SimPU, so it takes no lock.
// Frames are popped one per Sim frame so SelectionInput's press/release edge detection sees every state.

constexpr size_t kPointerQueueCapacity = 512;
constexpr size_t kPointerMaxFramesPerCall = 256;

// Returns true and fills outFrames on success; otherwise false with outError set (e.g. "frame 3: missing or non-numeric 'x'").
inline bool ParsePointerFrames(const Json::Value& params, std::vector<Dia::CellLayers::SelectionInput::PointerFrame>& outFrames, std::string& outError)
{
	using Frame = Dia::CellLayers::SelectionInput::PointerFrame;
	outFrames.clear();
	if (!params.isObject() || !params["frames"].isArray())
	{
		outError = "missing or non-array param 'frames'";
		return false;
	}
	const Json::Value& arr = params["frames"];
	if (arr.empty())
	{
		outError = "param 'frames' is empty";
		return false;
	}
	if (arr.size() > kPointerMaxFramesPerCall)
	{
		outError = "too many frames (max " + std::to_string(kPointerMaxFramesPerCall) + " per call)";
		return false;
	}
	for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
	{
		const Json::Value& j = arr[i];
		const std::string prefix = "frame " + std::to_string(i) + ": ";
		if (!j.isObject())
		{
			outError = prefix + "not an object";
			return false;
		}
		if (!j["x"].isNumeric()) { outError = prefix + "missing or non-numeric 'x'"; return false; }
		if (!j["y"].isNumeric()) { outError = prefix + "missing or non-numeric 'y'"; return false; }
		bool flags[7] = {};
		static const char* const kNames[7] = { "left", "middle", "right", "shift", "ctrl", "alt", "esc" };
		for (int k = 0; k < 7; ++k)
		{
			if (!j.isMember(kNames[k])) continue;
			if (!j[kNames[k]].isBool())
			{
				outError = prefix + "non-bool '" + kNames[k] + "'";
				return false;
			}
			flags[k] = j[kNames[k]].asBool();
		}
		Frame f;
		f.x = j["x"].asFloat();
		f.y = j["y"].asFloat();
		f.left = flags[0]; f.middle = flags[1]; f.right = flags[2];
		f.shift = flags[3]; f.ctrl = flags[4]; f.alt = flags[5];
		f.escPressed = flags[6];
		f.hasKeyboard = true;
		f.valid = true;
		outFrames.push_back(f);
	}
	return true;
}

class PointerFrameQueue
{
public:
	using Frame = Dia::CellLayers::SelectionInput::PointerFrame;

	// All-or-nothing: returns false (queue unchanged) if the whole batch does not fit.
	bool TryEnqueue(const std::vector<Frame>& frames)
	{
		if (mFrames.size() - mHead + frames.size() > kPointerQueueCapacity)
			return false;
		Compact();
		mFrames.insert(mFrames.end(), frames.begin(), frames.end());
		return true;
	}

	bool TryPop(Frame& out)
	{
		if (mHead >= mFrames.size())
			return false;
		out = mFrames[mHead++];
		return true;
	}

	size_t Pending() const
	{
		return mFrames.size() - mHead;
	}

	void Clear()
	{
		mFrames.clear();
		mHead = 0;
	}

private:
	void Compact()
	{
		if (mHead > 0)
		{
			mFrames.erase(mFrames.begin(), mFrames.begin() + static_cast<std::ptrdiff_t>(mHead));
			mHead = 0;
		}
	}

	std::vector<Frame> mFrames;
	size_t mHead = 0;
};

#ifdef DIA_DEBUG
// Registers cluichetest.celllayers.pointer_frames (command), .pointer_pending and .cell_pixel (reads) on the sim-hosted registrar.
// Runs on SimPU. `queue` and whatever `cellPixel` captures must outlive the registration. Returns false (and unregisters any it had
// registered) if the registrar rejects one.
inline void UnregisterPointerInjectionCommands(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
{
	registrar.Unregister("cluichetest.celllayers.pointer_frames");
	registrar.Unregister("cluichetest.celllayers.pointer_pending");
	registrar.Unregister("cluichetest.celllayers.cell_pixel");
}

inline bool RegisterPointerInjectionCommands(Dia::CellLayers::IMirrorQueryRegistrar& registrar, PointerFrameQueue& queue,
	Dia::CellLayers::MirrorQueryHandler cellPixel)
{
	const bool ok =
		registrar.RegisterCommand("cluichetest.celllayers.pointer_frames", [&queue](const Json::Value& params) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			std::vector<Dia::CellLayers::SelectionInput::PointerFrame> parsed;
			std::string err;
			if (!ParsePointerFrames(params, parsed, err))
			{
				out["error"] = err;
				return out;
			}
			if (!queue.TryEnqueue(parsed))
			{
				out["error"] = "queue full";
				return out;
			}
			out["ok"] = true;
			out["queued"] = static_cast<Json::UInt>(parsed.size());
			out["pending"] = static_cast<Json::UInt>(queue.Pending());
			return out;
		}) &&
		registrar.Register("cluichetest.celllayers.pointer_pending", [&queue](const Json::Value&) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			out["pending"] = static_cast<Json::UInt>(queue.Pending());
			return out;
		}) &&
		registrar.Register("cluichetest.celllayers.cell_pixel", std::move(cellPixel));
	if (!ok)
		UnregisterPointerInjectionCommands(registrar);
	return ok;
}
#endif // DIA_DEBUG

} // namespace CluicheTest
