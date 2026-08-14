#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaAIBudget/IAIBudgetedSystem.h>

namespace Dia
{
	namespace AIBudget
	{
		// Maximum number of systems the scheduler can hold.
		// Declared at namespace scope so AIBudgetResult::perSystem can reference it
		// before AIBudgetScheduler is defined.  Re-exposed as AIBudgetScheduler::kMaxSystems
		// for external callers.
		static constexpr int kMaxSystems = 16;

#ifdef DIA_DEBUG
		//-------------------------------------------------------------------------------------------
		// SystemTimingEntry  (DIA_DEBUG only)
		//
		// Per-system timing record populated by AIBudgetScheduler::Update() each tick.
		// Omitted from Release builds to keep AIBudgetResult size unchanged (SD-001).
		//-------------------------------------------------------------------------------------------
		struct SystemTimingEntry
		{
			Dia::Core::StringCRC systemId;  // identity of the system
			float                timeMs;    // wall-clock time this system consumed this tick
			bool                 ran;       // false = deferred (received zero budget)
		};
#endif

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
#ifdef DIA_DEBUG
			Dia::Core::Containers::DynamicArrayC<SystemTimingEntry, kMaxSystems> perSystem;
#endif
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
			// Re-exposed as a class constant to preserve the AIBudgetScheduler::kMaxSystems API.
			static constexpr int kMaxSystems = ::Dia::AIBudget::kMaxSystems;

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

			// Returns the budgetMs value passed to the most recent Update() call.
			// Returns 0.0f if Update() has never been called.
			// Not DIA_DEBUG gated — useful for tests and non-debug diagnostics (SD-002).
			float GetLastBudgetMs() const;

		private:
			Dia::Core::Containers::DynamicArrayC<IAIBudgetedSystem*, kMaxSystems> mSystems;
			float mLastBudgetMs = 0.0f;
		};

	} // namespace AIBudget
} // namespace Dia
