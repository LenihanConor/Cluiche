#pragma once

#include <stdint.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/SerialExecutor.h>

namespace Dia::DeferredStep::Testing
{
	// Thread-free IStepLauncher. Launch records the step; Run happens only on CompleteNext/CompleteAll or an
	// inline Wait, so the stall path is deterministic. Every interface call is logged for sequencing asserts.
	class ManualStepLauncher final : public IStepLauncher
	{
	public:
		static constexpr uint32_t kCapacity = 64;

		struct Call
		{
			enum Kind : uint8_t { kLaunch, kIsComplete, kWait, kRelease };
			Kind kind = kLaunch;
			LaunchToken token;
		};

		LaunchToken Launch(IDeferredStep& step, const LaunchInfo& info) override
		{
			DIA_ASSERT(mEntryCount < kCapacity, "ManualStepLauncher capacity exceeded");
			if (mEntryCount >= kCapacity)
			{
				return LaunchToken();
			}
			Entry& e = mEntries[mEntryCount++];
			e = Entry();
			e.step = &step;
			e.info = info;
			mLastInfo = info;
			LaunchToken token;
			token.value = mEntryCount; // tokens start at 1 and equal index + 1
			Log(Call::kLaunch, token);
			return token;
		}

		bool IsComplete(LaunchToken token) const override
		{
			Log(Call::kIsComplete, token);
			const Entry* e = Find(token);
			return e && e->completed;
		}

		void Wait(LaunchToken token) override
		{
			Log(Call::kWait, token);
			Entry* e = Find(token);
			if (e && !e->completed)
			{
				RunEntry(*e);
			}
		}

		void Release(LaunchToken token) override
		{
			Log(Call::kRelease, token);
			Entry* e = Find(token);
			if (!e || e->released)
			{
				++mBadReleaseCount;
				if (mAssertOnBadRelease)
				{
					DIA_ASSERT(false, "ManualStepLauncher: Release of unknown or already-released token");
				}
				return;
			}
			e->released = true;
			++mReleaseCount;
		}

		// Runs the oldest launched-but-not-completed step. Returns false if none.
		bool CompleteNext()
		{
			for (uint32_t i = 0; i < mEntryCount; ++i)
			{
				if (!mEntries[i].completed)
				{
					RunEntry(mEntries[i]);
					return true;
				}
			}
			return false;
		}

		void CompleteAll()
		{
			while (CompleteNext()) {}
		}

		uint32_t GetLaunchCount() const { return mEntryCount; }
		const LaunchInfo& GetLastLaunchInfo() const { return mLastInfo; }
		uint32_t GetReleaseCount() const { return mReleaseCount; }
		uint32_t GetBadReleaseCount() const { return mBadReleaseCount; }
		bool IsReleased(LaunchToken token) const { const Entry* e = Find(token); return e && e->released; }
		IDeferredStep* GetStepForToken(LaunchToken token) const { const Entry* e = Find(token); return e ? e->step : nullptr; }
		void SetAssertOnBadRelease(bool enable) { mAssertOnBadRelease = enable; }

		uint32_t GetCallCount() const { return mCallCount; }
		const Call& GetCall(uint32_t i) const { return mCalls[i]; }
		void ClearCalls() { mCallCount = 0; }

	private:
		struct Entry
		{
			IDeferredStep* step = nullptr;
			LaunchInfo info;
			bool completed = false;
			bool released = false;
		};

		Entry* Find(LaunchToken token)
		{
			return (token.value >= 1 && token.value <= mEntryCount) ? &mEntries[token.value - 1] : nullptr;
		}
		const Entry* Find(LaunchToken token) const
		{
			return (token.value >= 1 && token.value <= mEntryCount) ? &mEntries[token.value - 1] : nullptr;
		}

		void RunEntry(Entry& e)
		{
			e.completed = true;
			e.step->Run(mSerialExecutor);
		}

		void Log(Call::Kind kind, LaunchToken token) const
		{
			if (mCallCount < kCapacity)
			{
				mCalls[mCallCount].kind = kind;
				mCalls[mCallCount].token = token;
				++mCallCount;
			}
		}

		Entry mEntries[kCapacity];
		uint32_t mEntryCount = 0;
		LaunchInfo mLastInfo;
		uint32_t mReleaseCount = 0;
		uint32_t mBadReleaseCount = 0;
		bool mAssertOnBadRelease = true;
		SerialExecutor mSerialExecutor;

		mutable Call mCalls[kCapacity];
		mutable uint32_t mCallCount = 0;
	};
}
