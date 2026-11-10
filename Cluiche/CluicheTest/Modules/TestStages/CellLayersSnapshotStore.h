#pragma once

// CellLayersSnapshotStore: the CellLayersViewer stage's ISnapshotStore. It saves the stage's REAL animated state to
// `<Folder>/<name>.state` so a restore is visible (the temperature layer and the sample cadence jump back):
//   dia.celllayers_stage_state v1
//   tempVersion=<u64>       the temperature layer version; every animated temperature value is a pure function of it
//   animateCounter=<u32>    frames since the last version step
//   historyFrame=<u32>      the stage's history sampling counter (sample phase)
// Only this store reads or writes the `.state` files (DiaCellHistory never does, spec AC 14).
//
// Contract: RequestTake / RequestRestore queue ONE request and run it in RunFrameBoundary(), which the stage calls once per
// Sim frame; the callback fires there. A second request while one is queued is refused: its callback fires with ok = false
// ("busy") at the same boundary, and the first request is untouched. Writes are atomic (temp file + rename). Same-PU only
// (SimPU); no locking. Header-only so GoogleTests can compile it without the stage.

#include <DiaCellHistory/ISnapshotStore.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace CluicheTest
{
	struct CellLayersStageState
	{
		uint64_t	tempVersion = 1;
		uint32_t	animateCounter = 0;
		uint32_t	historyFrame = 0;
	};

	class CellLayersSnapshotStore : public Dia::CellHistory::ISnapshotStore
	{
	public:
		using CaptureFn = std::function<CellLayersStageState()>;
		using ApplyFn = std::function<void(const CellLayersStageState&)>;

		CellLayersSnapshotStore(std::string folder, CaptureFn capture, ApplyFn apply)
			: mFolder(std::move(folder)), mCapture(std::move(capture)), mApply(std::move(apply)) {}

		static bool IsSafeName(const char* name)
		{
			if (name == nullptr || *name == '\0') return false;
			for (const char* p = name; *p != '\0'; ++p)
			{
				const char c = *p;
				const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
				if (!ok) return false;
			}
			return true;
		}

		void RequestTake(const char* name, Dia::CellHistory::SnapshotCallback cb, void* user) override { Queue(Kind::Take, name, cb, user); }
		void RequestRestore(const char* name, Dia::CellHistory::SnapshotCallback cb, void* user) override { Queue(Kind::Restore, name, cb, user); }

		bool Exists(const char* name) const override
		{
			if (!IsSafeName(name)) return false;
			std::error_code ec;
			return std::filesystem::is_regular_file(StatePath(name), ec);
		}

		bool Delete(const char* name) override
		{
			if (!IsSafeName(name)) return false;
			std::error_code ec;
			return std::filesystem::remove(StatePath(name), ec) && !ec;
		}

		const char* Folder() const override { return mFolder.c_str(); }

		bool HasPending() const { return mHasPending; }

		// Runs the queued request (and fires the refusals queued behind it). Call once per Sim frame, before the stage mutates state.
		void RunFrameBoundary()
		{
			if (mHasPending)
			{
				const Request r = mPending;
				mHasPending = false;
				std::string message;
				const bool ok = (r.kind == Kind::Take) ? DoTake(r.name, message) : DoRestore(r.name, message);
				if (r.cb != nullptr) r.cb(ok, message.c_str(), r.user);
			}
			if (!mRejected.empty())
			{
				std::vector<Request> rejected;
				rejected.swap(mRejected);
				for (const Request& r : rejected)
					if (r.cb != nullptr) r.cb(false, "busy: another snapshot request is pending", r.user);
			}
		}

		static bool Serialise(const CellLayersStageState& s, std::string& out)
		{
			char buf[160];
			std::snprintf(buf, sizeof(buf), "dia.celllayers_stage_state v1\ntempVersion=%llu\nanimateCounter=%u\nhistoryFrame=%u\n",
				static_cast<unsigned long long>(s.tempVersion), s.animateCounter, s.historyFrame);
			out = buf;
			return true;
		}

		static bool Parse(const std::string& text, CellLayersStageState& s)
		{
			std::istringstream in(text);
			std::string line;
			if (!std::getline(in, line) || line != "dia.celllayers_stage_state v1") return false;
			bool gotV = false, gotA = false, gotH = false;
			CellLayersStageState tmp;
			while (std::getline(in, line))
			{
				const size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				const std::string key = line.substr(0, eq);
				const std::string val = line.substr(eq + 1);
				if (val.empty() || val.find_first_not_of("0123456789") != std::string::npos) return false;
				if (key == "tempVersion") { tmp.tempVersion = std::strtoull(val.c_str(), nullptr, 10); gotV = true; }
				else if (key == "animateCounter") { tmp.animateCounter = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10)); gotA = true; }
				else if (key == "historyFrame") { tmp.historyFrame = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10)); gotH = true; }
			}
			if (!(gotV && gotA && gotH)) return false;
			s = tmp;
			return true;
		}

	private:
		enum class Kind { Take, Restore };
		struct Request { Kind kind; std::string name; Dia::CellHistory::SnapshotCallback cb; void* user; };

		void Queue(Kind kind, const char* name, Dia::CellHistory::SnapshotCallback cb, void* user)
		{
			Request r{ kind, name != nullptr ? name : "", cb, user };
			if (mHasPending) mRejected.push_back(r);
			else { mPending = r; mHasPending = true; }
		}

		std::string StatePath(const std::string& name) const { return mFolder + "/" + name + ".state"; }

		bool DoTake(const std::string& name, std::string& message)
		{
			if (!IsSafeName(name.c_str())) { message = "invalid snapshot name"; return false; }
			if (!mCapture) { message = "no state source"; return false; }
			std::string text;
			Serialise(mCapture(), text);
			std::error_code ec;
			std::filesystem::create_directories(mFolder, ec);
			if (ec) { message = "cannot create snapshot folder: " + ec.message(); return false; }
			const std::string finalPath = StatePath(name);
			const std::string tmpPath = finalPath + ".tmp";
			{
				std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
				if (!f) { message = "cannot write " + tmpPath; return false; }
				f.write(text.data(), static_cast<std::streamsize>(text.size()));
				f.flush();
				if (!f) { f.close(); std::filesystem::remove(tmpPath, ec); message = "write failed for " + tmpPath; return false; }
			}
			std::filesystem::rename(tmpPath, finalPath, ec);
			if (ec)
			{
				std::error_code ec2;
				std::filesystem::remove(tmpPath, ec2);
				message = "rename failed: " + ec.message();
				return false;
			}
			message = "ok";
			return true;
		}

		bool DoRestore(const std::string& name, std::string& message)
		{
			if (!IsSafeName(name.c_str())) { message = "invalid snapshot name"; return false; }
			if (!mApply) { message = "no state sink"; return false; }
			std::ifstream f(StatePath(name), std::ios::binary);
			if (!f) { message = "state file missing: " + name; return false; }
			std::stringstream ss;
			ss << f.rdbuf();
			CellLayersStageState s;
			if (!Parse(ss.str(), s)) { message = "state file corrupt: " + name; return false; }
			mApply(s);
			message = "ok";
			return true;
		}

		std::string		mFolder;
		CaptureFn		mCapture;
		ApplyFn			mApply;
		Request			mPending{ Kind::Take, std::string(), nullptr, nullptr };
		bool			mHasPending = false;
		std::vector<Request> mRejected;
	};
}
