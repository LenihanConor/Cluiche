#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaParallelExecutor/IParallelExecutor.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>

namespace Dia::DeferredStep::Testing
{
	// IDeferredStep with a fully inspectable, serializable working copy (the pending result) and committed state
	// (what readers see after Apply). Content is deterministic: Capture derives the working copy from mSeed, Run
	// hash-chains it, Apply commits it. Used to prove a save/load cycle is invisible (DFS-005).
	//
	// Apply records the key it landed on, read from the key source the test points it at (the value passed to Tick).
	// SerializePending/DeserializePending write the working copy as an int count plus kSize numbered int values.
	class PendingStateStep final : public IDeferredStep
	{
	public:
		static constexpr uint32_t kSize = 64;

		typedef std::vector<int32_t> State;
		// One entry per Apply: (step id, key it applied at). Shared across steps for multi-channel order checks.
		typedef std::vector<std::pair<int, StepKey>> ApplyLog;

		PendingStateStep() : mWorking(kSize, 0), mCommitted(kSize, 0) {}

		void Configure(int id, const StepKey* keySource, ApplyLog* sink)
		{
			mId = id;
			mKeySource = keySource;
			mApplySink = sink;
		}

		// Test hook, called from Run halfway through the hash chain (on the launcher's thread).
		void SetMidRunHook(std::function<void()> hook) { mMidRunHook = std::move(hook); }

		// SimPU-owned input that Capture copies. Set before Kick.
		void SetSeed(uint32_t seed) { mSeed = seed; }

		void Capture() override
		{
			++mCaptureCount;
			for (uint32_t i = 0; i < kSize; ++i)
			{
				mWorking[i] = static_cast<int32_t>(Mix(mSeed, i));
			}
		}

		void Run(Dia::ParallelExecutor::IParallelExecutor&) override
		{
			++mRunCount;
			uint32_t acc = 17;
			for (uint32_t i = 0; i < kSize; ++i)
			{
				if (i == kSize / 2 && mMidRunHook)
				{
					mMidRunHook();
				}
				acc = acc * 31u + static_cast<uint32_t>(mWorking[i]);
				mWorking[i] = static_cast<int32_t>(Mix(acc, i));
			}
		}

		void Apply() override
		{
			++mApplyCount;
			mCommitted = mWorking;
			const StepKey key = (mKeySource != nullptr) ? *mKeySource : 0;
			mAppliedKeys.push_back(key);
			if (mApplySink != nullptr)
			{
				mApplySink->push_back(std::make_pair(mId, key));
			}
		}

		uint32_t GetPendingVersion() const override { return 1; }

		bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const override
		{
			int32_t count = 0;
			return ctx.Read(Dia::Core::StringCRC("count"), count) && count == static_cast<int32_t>(kSize);
		}

		void SerializePending(Dia::SaveGame::SaveContext& ctx) const override
		{
			WriteState(ctx, mWorking);
		}

		void DeserializePending(Dia::SaveGame::LoadContext& ctx) override
		{
			++mDeserializeCount;
			ReadState(ctx, mWorking);
		}

		// Committed state helpers, for the consumer that owns it (and its own save).
		static void WriteState(Dia::SaveGame::SaveContext& ctx, const State& state)
		{
			ctx.Write(Dia::Core::StringCRC("count"), static_cast<int32_t>(state.size()));
			for (uint32_t i = 0; i < state.size(); ++i)
			{
				char name[16];
				snprintf(name, sizeof(name), "v%u", i);
				ctx.Write(Dia::Core::StringCRC(name), state[i]);
			}
		}

		static bool ReadState(Dia::SaveGame::LoadContext& ctx, State& state)
		{
			int32_t count = 0;
			if (!ctx.Read(Dia::Core::StringCRC("count"), count) || count < 0)
			{
				return false;
			}
			state.assign(static_cast<size_t>(count), 0);
			bool ok = true;
			for (int32_t i = 0; i < count; ++i)
			{
				char name[16];
				snprintf(name, sizeof(name), "v%d", i);
				ok = ctx.Read(Dia::Core::StringCRC(name), state[static_cast<size_t>(i)]) && ok;
			}
			return ok;
		}

		const State& GetWorking() const { return mWorking; }
		const State& GetCommitted() const { return mCommitted; }
		void SetCommitted(const State& state) { mCommitted = state; }
		bool HasCommitted() const { return mApplyCount > 0 || mCommitted != State(kSize, 0); }

		// Canonical byte string of the committed state: exactly the bytes of the values, in order.
		std::string GetCommittedBytes() const
		{
			std::string bytes(mCommitted.size() * sizeof(int32_t), '\0');
			if (!mCommitted.empty())
			{
				memcpy(&bytes[0], mCommitted.data(), bytes.size());
			}
			return bytes;
		}

		const std::vector<StepKey>& GetAppliedKeys() const { return mAppliedKeys; }
		uint32_t GetCaptureCount() const { return mCaptureCount; }
		uint32_t GetRunCount() const { return mRunCount; }
		uint32_t GetApplyCount() const { return mApplyCount; }
		uint32_t GetDeserializeCount() const { return mDeserializeCount; }

	private:
		static uint32_t Mix(uint32_t seed, uint32_t index)
		{
			uint32_t x = seed * 2654435761u + index * 40503u + 12345u;
			x ^= x >> 15;
			x *= 2246822519u;
			x ^= x >> 13;
			return x;
		}

		int mId = 0;
		const StepKey* mKeySource = nullptr;
		ApplyLog* mApplySink = nullptr;
		std::function<void()> mMidRunHook;
		uint32_t mSeed = 0;
		State mWorking;
		State mCommitted;
		std::vector<StepKey> mAppliedKeys;
		uint32_t mCaptureCount = 0;
		uint32_t mRunCount = 0;
		uint32_t mApplyCount = 0;
		uint32_t mDeserializeCount = 0;
	};
}
