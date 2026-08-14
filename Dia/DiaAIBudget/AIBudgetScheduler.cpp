#include "AIBudgetScheduler.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <chrono>
#include <algorithm>

namespace Dia
{
	namespace AIBudget
	{
		bool AIBudgetScheduler::Register(IAIBudgetedSystem* system)
		{
			DIA_ASSERT(system != nullptr, "AIBudgetScheduler::Register — system pointer must not be null");
			if (system == nullptr)
			{
				DIA_LOG_ERROR("AIBudget", "Register failed: null system pointer");
				return false;
			}

			// Duplicate registration is always a caller bug — a system double-budgeted will
			// run twice per frame and its elapsed time will be counted twice.
			for (unsigned int i = 0; i < mSystems.Size(); ++i)
			{
				DIA_ASSERT(mSystems[i] != system,
					"AIBudgetScheduler::Register — system '%s' is already registered",
					system->GetSystemId().AsChar());
				if (mSystems[i] == system)
				{
					DIA_LOG_ERROR("AIBudget", "Register failed: system '%s' already registered", system->GetSystemId().AsChar());
					return false;
				}
			}

			DIA_ASSERT(!mSystems.IsFull(), "AIBudgetScheduler::Register — scheduler is at capacity (%d systems)", kMaxSystems);
			if (mSystems.IsFull())
			{
				DIA_LOG_ERROR("AIBudget", "Register failed: scheduler is at capacity (%d systems)", kMaxSystems);
				return false;
			}

			mSystems.Add(system);
			DIA_LOG_INFO("AIBudget", "Registered system: %s", system->GetSystemId().AsChar());
			return true;
		}

		void AIBudgetScheduler::Unregister(IAIBudgetedSystem* system)
		{
			DIA_ASSERT(system != nullptr, "AIBudgetScheduler::Unregister — system pointer must not be null");
			if (system == nullptr)
				return;

			for (unsigned int i = 0; i < mSystems.Size(); ++i)
			{
				if (mSystems[i] == system)
				{
					mSystems.RemoveAt(i);
					DIA_LOG_INFO("AIBudget", "Unregistered system");
					return;
				}
			}
			// Not found — no-op, no crash
		}

		AIBudgetResult AIBudgetScheduler::Update(float totalBudgetMs)
		{
			DIA_ASSERT(totalBudgetMs >= 0.0f, "AIBudgetScheduler::Update — totalBudgetMs must not be negative (got %.4f)", totalBudgetMs);

			mLastBudgetMs = totalBudgetMs;

			float elapsedSoFarMs = 0.0f;
			int systemsRun      = 0;
			int systemsDeferred = 0;

			AIBudgetResult result{};

			for (unsigned int i = 0; i < mSystems.Size(); ++i)
			{
				DIA_ASSERT(mSystems[i] != nullptr, "AIBudgetScheduler::Update — null system pointer at index %u (use Unregister before destroying a system)", i);
				if (mSystems[i] == nullptr)
				{
					++systemsDeferred;
#ifdef DIA_DEBUG
					result.perSystem.Add(SystemTimingEntry{ Dia::Core::StringCRC(), 0.0f, false });
#endif
					continue;
				}

				const float remaining = totalBudgetMs - elapsedSoFarMs;

				if (remaining <= 0.0f)
				{
#ifdef DIA_DEBUG
					// In debug, keep looping so every remaining system gets a perSystem entry.
					result.perSystem.Add(SystemTimingEntry{ mSystems[i]->GetSystemId(), 0.0f, false });
					++systemsDeferred;
					continue;
#else
					// Budget exhausted — skip this system and all remaining ones.
					systemsDeferred = static_cast<int>(mSystems.Size()) - static_cast<int>(i);
					break;
#endif
				}

				auto t0 = std::chrono::steady_clock::now();
				mSystems[i]->UpdateBudgeted(remaining);
				auto t1 = std::chrono::steady_clock::now();

				const float timeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
				elapsedSoFarMs += timeMs;
				++systemsRun;

#ifdef DIA_DEBUG
				result.perSystem.Add(SystemTimingEntry{ mSystems[i]->GetSystemId(), timeMs, true });
#endif
			}

			if (systemsDeferred > 0)
			{
				DIA_LOG_WARNING("AIBudget", "Budget exhausted: %d systems deferred", systemsDeferred);
			}

			result.systemsRun      = systemsRun;
			result.systemsDeferred = systemsDeferred;
			result.usedMs          = elapsedSoFarMs;
			return result;
		}

		int AIBudgetScheduler::GetRegisteredCount() const
		{
			return static_cast<int>(mSystems.Size());
		}

		float AIBudgetScheduler::GetLastBudgetMs() const
		{
			return mLastBudgetMs;
		}

	} // namespace AIBudget
} // namespace Dia
