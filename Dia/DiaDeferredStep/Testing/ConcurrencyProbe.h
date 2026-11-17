#pragma once

#include <atomic>

namespace Dia::DeferredStep::Testing
{
	// Counts how many bodies are executing at once and remembers the peak. Tests assert on Peak()
	// instead of sleeping, so cap checks do not flake under load.
	class ConcurrencyProbe
	{
	public:
		void Enter()
		{
			const int now = mCurrent.fetch_add(1) + 1;
			int peak = mPeak.load();
			while (now > peak && !mPeak.compare_exchange_weak(peak, now)) {}
		}

		void Exit() { mCurrent.fetch_sub(1); }

		int Current() const { return mCurrent.load(); }
		int Peak()    const { return mPeak.load(); }

		class Scope
		{
		public:
			explicit Scope(ConcurrencyProbe& probe) : mProbe(probe) { mProbe.Enter(); }
			~Scope() { mProbe.Exit(); }
			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;
		private:
			ConcurrencyProbe& mProbe;
		};

	private:
		std::atomic<int> mCurrent{ 0 };
		std::atomic<int> mPeak{ 0 };
	};
}
