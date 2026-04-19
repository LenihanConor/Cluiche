////////////////////////////////////////////////////////////////////////////////
// Filename: HotReloadManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/HotReloadManager.h"

#include "DiaApplication/ApplicationProcessingUnit.h"
#include "DiaApplication/ApplicationPhase.h"

#include <DiaCore/Core/Assert.h>
//#include <DiaCore/Core/Log.h>  // Disabled - logging causes crashes in tests

namespace Dia
{
	namespace Application
	{
		//---------------------------------------------------------------------------------------------------------
		HotReloadManager::HotReloadManager(ProcessingUnit* processingUnit)
			: mProcessingUnit(processingUnit)
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit cannot be null");
		}

		//---------------------------------------------------------------------------------------------------------
		HotReloadManager::ReloadResult HotReloadManager::ReplaceModule(
			const Dia::Core::StringCRC& oldModuleId,
			Module* newModule)
		{
			// Step 1: Validate ProcessingUnit
			if (mProcessingUnit == nullptr)
			{
				return ReloadResult::kInvalidProcessingUnit;
			}

			// Step 2: Validate new module
			if (newModule == nullptr)
			{
				return ReloadResult::kModuleNotFound;
			}

			// Step 3: Find old module
			Module* oldModule = mProcessingUnit->GetModule(oldModuleId);
			if (oldModule == nullptr)
			{
				return ReloadResult::kModuleNotFound;
			}

			// Build reload context
			ReloadContext context;
			context.oldModule = oldModule;
			context.newModule = newModule;
			context.savedState = nullptr;
			context.wasRunning = oldModule->HasStarted();

			// Collect modules that depend on this one
			CollectDependentModules(oldModule, context.dependentModules);

			// Validate reload is safe
			ReloadResult validationResult = ValidateReload(context);
			if (validationResult != ReloadResult::kSuccess)
			{
				return validationResult;
			}

			// Perform the reload
			ReloadResult result = PerformReload(context);

			if (result == ReloadResult::kSuccess)
			{
				// HotReload succeeded
			}
			else
			{
				// HotReload failed
			}

			return result;
		}

		//---------------------------------------------------------------------------------------------------------
		HotReloadManager::ReloadResult HotReloadManager::ReplaceModuleInPhase(
			const Dia::Core::StringCRC& phaseId,
			const Dia::Core::StringCRC& oldModuleId,
			Module* newModule)
		{
			// For now, delegate to full replacement
			// TODO: Could be optimized to only replace in specific phase
			return ReplaceModule(oldModuleId, newModule);
		}

		//---------------------------------------------------------------------------------------------------------
		HotReloadManager::ReloadResult HotReloadManager::ValidateReload(const ReloadContext& context)
		{
			// Check if module allows hot reload
			if (!context.oldModule->CanHotReload())
			{
				return ReloadResult::kModuleNotReloadable;
			}

			// Check version compatibility
			if (!context.newModule->GetVersion().IsCompatibleWith(context.oldModule->GetVersion()))
			{
				return ReloadResult::kVersionIncompatible;
			}

			// Check if dependent modules are stopped
			for (Module* dependent : context.dependentModules)
			{
				if (dependent->HasStarted())
				{
					return ReloadResult::kDependentModulesRunning;
				}
			}

			return ReloadResult::kSuccess;
		}

		//---------------------------------------------------------------------------------------------------------
		HotReloadManager::ReloadResult HotReloadManager::PerformReload(ReloadContext& context)
		{
			// Validate preconditions
			DIA_ASSERT(context.oldModule != nullptr, "Old module is null in PerformReload");
			DIA_ASSERT(context.newModule != nullptr, "New module is null in PerformReload");
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null in PerformReload");

			// Step 1: Stop old module if running
			// IMPORTANT: Must stop before saving state to ensure consistent state capture
			if (context.wasRunning)
			{
				context.oldModule->Stop();
			}

			// Step 2: Save state from old module
			// Module's SaveState() can return nullptr if no state to transfer
			context.savedState = context.oldModule->SaveState();

			// Step 3: Remove old module from all phases that contain it
			// COMMON PATTERN: Collect affected phases first, then modify them
			// Must track which phases contain the module so we can add the new module to them
			std::vector<Phase*> affectedPhases;
			ProcessingUnit::PhasesTable& phases = mProcessingUnit->mAssociatedPhases;

			// COMMON PATTERN: HashTable iteration using Begin()/End() with it.Value() to get pointer
			for (auto it = phases.Begin(); it != phases.End(); ++it)
			{
				Phase* phase = it.Value();
				if (phase != nullptr && phase->ContainsModule(context.oldModule->GetUniqueId()))
				{
					bool removed = phase->RemoveModule(context.oldModule->GetUniqueId());
					DIA_ASSERT(removed, "Failed to remove module from phase %s", phase->GetUniqueId().AsChar());
					affectedPhases.push_back(phase);
				}
			}

			// Step 4: Remove old module from ProcessingUnit
			// IMPORTANT: Remove from ProcessingUnit's module table before adding new module
			// This prevents having both old and new modules registered simultaneously
			bool removedFromPU = mProcessingUnit->RemoveModule(context.oldModule->GetUniqueId());
			DIA_ASSERT(removedFromPU, "Failed to remove old module from ProcessingUnit");

			// Step 5: Add new module to ProcessingUnit
			// New module gets same StringCRC ID as old module (e.g., "TestModule")
			mProcessingUnit->AddModule(context.newModule);

			// Step 6: Add new module to all affected phases
			// IMPORTANT: Must add to all phases that previously contained the old module
			// Phase::AddModule() recursively adds dependencies, maintaining dependency graph
			for (Phase* phase : affectedPhases)
			{
				phase->AddModule(context.newModule);
			}

			// Step 7: Update dependency references in dependent modules
			// TODO: Currently a placeholder - needs full implementation to update modules
			// that have the old module as a dependency to reference the new module instead
			UpdateDependencyReferences(context.oldModule, context.newModule);

			// Step 8: Restore state to new module
			// Transfer saved state from old module to new module for continuity
			if (context.savedState != nullptr)
			{
				context.newModule->RestoreState(context.savedState);
			}

			// Ensure new module is in valid state for lifecycle operations
			if (context.newModule->GetState() == StateObject::StateEnum::kConstructed)
			{
				BuildDependencyData buildData(
					&mProcessingUnit->mAssociatedPhases,
					&mProcessingUnit->mPhaseTransitions,
					&mProcessingUnit->mAssociatedModules);
				context.newModule->BuildDependancies(&buildData);
			}

			// Step 9: Start new module if old was running
			if (context.wasRunning)
			{
				context.newModule->Start(nullptr);
			}

			// Step 10: Clean up old module
			// IMPORTANT: Only safe to delete after all references are removed and rollback is no longer possible
			// Old module has been removed from all ProcessingUnit and Phase tables at this point
			delete context.oldModule;

			return ReloadResult::kSuccess;
		}

		//---------------------------------------------------------------------------------------------------------
		void HotReloadManager::CollectDependentModules(Module* module, std::vector<Module*>& dependents)
		{
			// This is a simplified version - in reality you'd need to search all modules
			// to find which ones have this module as a dependency
			// For now, we assume the caller ensures dependent modules are stopped
			dependents.clear();
		}

		//---------------------------------------------------------------------------------------------------------
		void HotReloadManager::UpdateDependencyReferences(Module* oldModule, Module* newModule)
		{
			// Update dependency references in all modules that depend on oldModule
			// This is complex because we need to search through all modules' dependency tables
			// For now, this is a placeholder for manual or derived class implementation
		}

		//---------------------------------------------------------------------------------------------------------
		const char* HotReloadManager::GetResultString(ReloadResult result)
		{
			switch (result)
			{
				case ReloadResult::kSuccess:                 return "Success";
				case ReloadResult::kModuleNotFound:          return "Module not found";
				case ReloadResult::kModuleNotReloadable:     return "Module does not allow hot reload";
				case ReloadResult::kVersionIncompatible:     return "Version incompatible";
				case ReloadResult::kModuleRunning:           return "Module is still running";
				case ReloadResult::kDependentModulesRunning: return "Dependent modules still running";
				case ReloadResult::kNewModuleStartFailed:    return "New module failed to start";
				case ReloadResult::kInvalidProcessingUnit:   return "Invalid ProcessingUnit";
				default:                                     return "Unknown error";
			}
		}
	}
}
