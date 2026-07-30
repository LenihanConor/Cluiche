#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace AIBudget
	{
		//-------------------------------------------------------------------------------------------
		// IAIBudgetedSystem
		//
		// Interface for any AI subsystem that can receive a time budget slice from AIBudgetScheduler.
		// Implementations must handle budgetMs == 0.0f gracefully (no-op is acceptable).
		//-------------------------------------------------------------------------------------------
		class IAIBudgetedSystem
		{
		public:
			virtual ~IAIBudgetedSystem() = default;

			// Returns a unique identifier for this system (used for logging).
			virtual Dia::Core::StringCRC GetSystemId() const = 0;

			// Called each frame with the remaining budget in milliseconds.
			// budgetMs may be 0.0f if the budget is already exhausted; implementations must handle this gracefully.
			virtual void UpdateBudgeted(float budgetMs) = 0;
		};

	} // namespace AIBudget
} // namespace Dia
