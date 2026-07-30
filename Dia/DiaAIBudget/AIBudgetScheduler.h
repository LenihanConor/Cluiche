#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaAIBudget/IAIBudgetedSystem.h>

namespace Dia
{
	namespace AIBudget
	{
		//-------------------------------------------------------------------------------------------
		// AIBudgetResult
		//
		// Returned by AIBudgetScheduler::Update() to surface per-frame scheduling telemetry.
		//-------------------------------------------------------------------------------------------
		struct AIBudgetResult
		{
			int   systemsRun;       // systems that received a non-zero budget slice
			int   systemsDeferred;  // systems skipped because budget exhausted
			float usedMs;           // total wall-clock time consumed by all systems this tick
		};

		//-------------------------------------------------------------------------------------------
		// AIBudgetScheduler
		//
		// Owns a fixed-capacity list of IAIBudgetedSystem pointers and distributes a per-frame
		// time budget across them in registration order.  Systems are called with the remaining
		// budget; once the budget is exhausted (remaining <= 0) further systems are skipped.
		//
		// Owned by AIBudgetModule — not a singleton.
		//-------------------------------------------------------------------------------------------
		class AIBudgetScheduler
		{
		public:
			static constexpr int kMaxSystems = 16;

			AIBudgetScheduler() = default;

			// Register a budgeted system.  Returns true on success.
			// Returns false (and logs an error) if the scheduler is already at capacity.
			// Debug builds assert on nullptr or full capacity.
			bool Register(IAIBudgetedSystem* system);

			// Remove a previously registered system by pointer identity.  No-op if not found.
			void Unregister(IAIBudgetedSystem* system);

			// Distribute totalBudgetMs across registered systems in order.
			// Measures wall-clock time consumed by each system and passes remaining budget to the next.
			// Systems are skipped (not called) once the budget is exhausted.
			// Returns per-frame telemetry for use by AIBudgetModule metrics.
			AIBudgetResult Update(float totalBudgetMs);

			// Returns the number of currently registered systems.
			int GetRegisteredCount() const;

		private:
			Dia::Core::Containers::DynamicArrayC<IAIBudgetedSystem*, kMaxSystems> mSystems;
		};

	} // namespace AIBudget
} // namespace Dia
