#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaAIBudget/AIBudgetScheduler.h>

// Forward declarations — avoids pulling full metric headers into every translation unit.
namespace Dia { namespace Observation { namespace Metric {
	class Gauge;
	class Counter;
} } }

namespace Dia
{
	namespace AIBudget
	{
		//-------------------------------------------------------------------------------------------
		// AIBudgetModule
		//
		// ApplicationFlow::Module that owns an AIBudgetScheduler and drives it each frame.
		// Place on SimPU.  Configure via manifest JSON: { "budgetUs": 1000 }
		//
		// Registers three DiaObservation metrics on Start:
		//   ai.budget.used_us        (Gauge)   — wall-clock microseconds consumed by AI systems
		//   ai.budget.systems_run    (Counter) — systems that executed this frame
		//   ai.budget.systems_deferred (Counter) — systems skipped due to budget exhaustion
		//-------------------------------------------------------------------------------------------
		class AIBudgetModule : public Dia::ApplicationFlow::SimModule
		{
		public:
			static const Dia::Core::StringCRC kInstanceId;

			AIBudgetModule();

			// Access the scheduler to register/unregister IAIBudgetedSystem instances.
			AIBudgetScheduler&       GetScheduler();
			const AIBudgetScheduler& GetScheduler() const;

			// Returns the result from the most recent DoUpdate() call.
			// Zero-initialised until the first frame runs.
			const AIBudgetResult& GetLastResult() const;

			// Returns the configured time budget in milliseconds.
			float GetBudgetMs() const;

		protected:
			// Reads "budgetUs" integer from JSON config.  Default: 1000 µs (= 1 ms).
			void        OnConfigure(const char* configJson) override;

			// Registers DiaObservation metrics.
			Dia::ApplicationFlow::StartResult DoStart() override;

			// Calls mScheduler.Update(mBudgetMs) and updates metrics.
			void        DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;

			// Nulls all metric pointers.
			Dia::ApplicationFlow::StopResult  DoStop() override;

		private:
			AIBudgetScheduler mScheduler;
			float             mBudgetMs;   // converted from budgetUs in OnConfigure; default 1.0f

			mutable AIBudgetResult mLastResult{};  // cached from most recent DoUpdate()

			Dia::Observation::Metric::Gauge*   mUsedUsGauge;
			Dia::Observation::Metric::Counter* mSystemsRunCounter;
			Dia::Observation::Metric::Counter* mSystemsDeferredCounter;
		};

	} // namespace AIBudget
} // namespace Dia
