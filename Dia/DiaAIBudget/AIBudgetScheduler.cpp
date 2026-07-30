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
			float elapsedSoFarMs = 0.0f;
			int systemsRun      = 0;
			int systemsDeferred = 0;

			for (unsigned int i = 0; i < mSystems.Size(); ++i)
			{
				const float remaining = totalBudgetMs - elapsedSoFarMs;

				if (remaining <= 0.0f)
				{
					// Budget exhausted — skip this system and all remaining ones
					systemsDeferred = static_cast<int>(mSystems.Size()) - static_cast<int>(i);
					break;
				}

				auto t0 = std::chrono::steady_clock::now();
				mSystems[i]->UpdateBudgeted(remaining);
				auto t1 = std::chrono::steady_clock::now();

				elapsedSoFarMs += std::chrono::duration<float, std::milli>(t1 - t0).count();
				++systemsRun;
			}

			if (systemsDeferred > 0)
			{
				DIA_LOG_WARNING("AIBudget", "Budget exhausted: %d systems deferred", systemsDeferred);
			}

			return AIBudgetResult{ systemsRun, systemsDeferred, elapsedSoFarMs };
		}

		int AIBudgetScheduler::GetRegisteredCount() const
		{
			return static_cast<int>(mSystems.Size());
		}

	} // namespace AIBudget
} // namespace Dia
