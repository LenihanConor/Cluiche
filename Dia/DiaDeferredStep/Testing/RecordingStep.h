#pragma once

#include <stdint.h>
#include <functional>
#include <thread>
#include <vector>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IParallelExecutor.h>

namespace Dia::DeferredStep::Testing
{
	// IDeferredStep that counts Capture/Run/Apply and records the thread id of the last call of each.
	// Order recording: if constructed with an orderSink, Apply appends this step's id to *orderSink,
	// so multi-channel tests can assert Apply order. Run optionally invokes an onRun hook.
	class RecordingStep final : public IDeferredStep
	{
	public:
		RecordingStep() = default;
		RecordingStep(int id, std::vector<int>* orderSink) : mId(id), mOrderSink(orderSink) {}

		void Capture() override
		{
			++mCaptureCount;
			mCaptureThread = std::this_thread::get_id();
		}

		void Run(IParallelExecutor& executor) override
		{
			++mRunCount;
			mRunThread = std::this_thread::get_id();
			mLastExecutor = &executor;
			if (mOnRun)
			{
				mOnRun();
			}
		}

		void Apply() override
		{
			++mApplyCount;
			mApplyThread = std::this_thread::get_id();
			if (mOrderSink)
			{
				mOrderSink->push_back(mId);
			}
		}

		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		uint32_t GetCaptureCount() const { return mCaptureCount; }
		uint32_t GetRunCount() const { return mRunCount; }
		uint32_t GetApplyCount() const { return mApplyCount; }
		std::thread::id GetCaptureThread() const { return mCaptureThread; }
		std::thread::id GetRunThread() const { return mRunThread; }
		std::thread::id GetApplyThread() const { return mApplyThread; }
		IParallelExecutor* GetLastExecutor() const { return mLastExecutor; }
		int GetId() const { return mId; }

		void SetOnRun(std::function<void()> fn) { mOnRun = std::move(fn); }

	private:
		int mId = 0;
		std::vector<int>* mOrderSink = nullptr;
		uint32_t mCaptureCount = 0;
		uint32_t mRunCount = 0;
		uint32_t mApplyCount = 0;
		std::thread::id mCaptureThread;
		std::thread::id mRunThread;
		std::thread::id mApplyThread;
		IParallelExecutor* mLastExecutor = nullptr;
		std::function<void()> mOnRun;
	};
}
