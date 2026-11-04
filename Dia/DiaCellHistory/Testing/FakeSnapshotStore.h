#pragma once

// Test helper (Testing/ convention): a scripted ISnapshotStore.
//  - Requests are QUEUED and executed by RunFrameBoundary(), which fires each callback (so the asynchronous contract is
//    testable). Every request is recorded in Calls() in execution order.
//  - FailNext(message) scripts the next executed request to fail (FIFO); otherwise requests succeed with "ok".
//  - A take adds the name to the existing set; optionally (SetWriteStateFiles) it also writes a real small
//    `<Folder>/<name>.state` file so "both halves" clean-up can be checked on disk. SetLeavePartialOnFail leaves that file
//    (and the name) behind on a scripted failure, like a store that failed half way.
//  - Folder() is a per-store temp directory (TempFolder), removed on destruction.

#include <DiaCellHistory/ISnapshotStore.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace Dia::CellHistory::Testing
{
	// RAII unique directory under the OS temp folder.
	class TempFolder
	{
	public:
		TempFolder()
		{
			static std::atomic<unsigned> sCounter{ 0 };
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			mPath = (std::filesystem::temp_directory_path() / ("dia_cellhistory_" + std::to_string(stamp) + "_" + std::to_string(sCounter++))).string();
			std::filesystem::create_directories(mPath);
		}
		~TempFolder() { std::error_code ec; std::filesystem::remove_all(mPath, ec); }
		TempFolder(const TempFolder&) = delete;
		TempFolder& operator=(const TempFolder&) = delete;
		const std::string& Path() const { return mPath; }
		std::string File(const std::string& name) const { return mPath + "/" + name; }
	private:
		std::string mPath;
	};

	class FakeSnapshotStore : public ISnapshotStore
	{
	public:
		enum class CallKind { Take, Restore };
		struct Call { CallKind kind; std::string name; bool ok; std::string message; };

		void RequestTake(const char* name, SnapshotCallback cb, void* user) override { mPending.push_back({ CallKind::Take, name ? name : "", cb, user }); }
		void RequestRestore(const char* name, SnapshotCallback cb, void* user) override { mPending.push_back({ CallKind::Restore, name ? name : "", cb, user }); }
		bool Exists(const char* name) const override { return name != nullptr && mExisting.count(name) != 0; }
		bool Delete(const char* name) override
		{
			if (name == nullptr || mExisting.erase(name) == 0) { return false; }
			std::error_code ec;
			std::filesystem::remove(StatePath(name), ec);
			return true;
		}
		const char* Folder() const override { return mTemp.Path().c_str(); }

		// Executes every queued request in order and fires its callback.
		void RunFrameBoundary()
		{
			while (!mPending.empty())
			{
				const Pending p = mPending.front();
				mPending.pop_front();
				bool ok = true;
				std::string message = "ok";
				if (!mScript.empty()) { ok = false; message = mScript.front(); mScript.pop_front(); }
				if (p.kind == CallKind::Take && (ok || mLeavePartialOnFail))
				{
					mExisting.insert(p.name);
					if (mWriteStateFiles) { std::ofstream(StatePath(p.name), std::ios::binary) << "state:" << p.name; }
				}
				mCalls.push_back({ p.kind, p.name, ok, message });
				if (p.cb != nullptr) { p.cb(ok, message.c_str(), p.user); }
			}
		}

		void FailNext(const char* message) { mScript.push_back(message ? message : "failed"); }
		void AddExisting(const char* name) { mExisting.insert(name); }
		void SetWriteStateFiles(bool on) { mWriteStateFiles = on; }
		void SetLeavePartialOnFail(bool on) { mLeavePartialOnFail = on; }
		std::string StatePath(const std::string& name) const { return mTemp.File(name + ".state"); }

		const std::vector<Call>& Calls() const { return mCalls; }
		size_t PendingCount() const { return mPending.size(); }

		// Forgets requests, calls, script and existing names; removes any state files this store wrote.
		void Clear()
		{
			for (const std::string& n : mExisting) { std::error_code ec; std::filesystem::remove(StatePath(n), ec); }
			mExisting.clear();
			mPending.clear();
			mCalls.clear();
			mScript.clear();
		}

	private:
		struct Pending { CallKind kind; std::string name; SnapshotCallback cb; void* user; };

		TempFolder				mTemp;
		std::deque<Pending>		mPending;
		std::deque<std::string>	mScript;
		std::set<std::string>	mExisting;
		std::vector<Call>		mCalls;
		bool					mWriteStateFiles = false;
		bool					mLeavePartialOnFail = false;
	};
}
