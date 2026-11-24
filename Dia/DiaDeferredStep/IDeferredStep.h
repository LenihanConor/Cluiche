#pragma once

#include <stdint.h>

namespace Dia::SaveGame
{
	class SaveContext;
	class LoadContext;
}

namespace Dia::ParallelExecutor
{
	class IParallelExecutor;
}

namespace Dia::DeferredStep
{
	class IDeferredStep
	{
	public:
		virtual ~IDeferredStep() = default;

		// SimPU, inside Kick. Copy every input into the working copy. After this returns, Run must not read SimPU-owned state.
		virtual void Capture() = 0;

		// Launcher's thread. Pure function of captured inputs; touches the working copy only. Result identical at any range count.
		virtual void Run(Dia::ParallelExecutor::IParallelExecutor& executor) = 0;

		// SimPU, at the apply key. Commit the working copy into the state readers see.
		virtual void Apply() = 0;

		// A completed-but-unapplied result; version is the step's own.
		// Called by the save participant's Validate for each saved channel; must not mutate anything. false aborts the load.
		virtual bool     ValidatePending(Dia::SaveGame::LoadContext& ctx) const { (void)ctx; return true; }
		virtual uint32_t GetPendingVersion() const = 0;
		virtual void     SerializePending(Dia::SaveGame::SaveContext& ctx) const = 0;
		virtual void     DeserializePending(Dia::SaveGame::LoadContext& ctx) = 0;
	};
}
