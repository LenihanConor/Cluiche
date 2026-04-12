////////////////////////////////////////////////////////////////////////////////
// Filename: HotReloadManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/HotReloadManager.h"

#include "DiaApplication/ApplicationProcessingUnit.h"
#include "DiaApplication/ApplicationPhase.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>

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
			if (mProcessingUnit == nullptr)
			{
				return ReloadResult::kInvalidProcessingUnit;
			}

			if (newModule == nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("ERROR: HotReload - new module is null");
				return ReloadResult::kModuleNotFound;
			}

			// Find old module
			Module* oldModule = mProcessingUnit->GetModule(oldModuleId);
			if (oldModule == nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("ERROR: HotReload - module %s not found",
				                                 oldModuleId.AsChar());
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
				Dia::Core::Log::OutputVaradicLine("HotReload SUCCESS: Module %s replaced (v%d.%d.%d -> v%d.%d.%d)",
				                                 oldModuleId.AsChar(),
				                                 oldModule->GetVersion().major,
				                                 oldModule->GetVersion().minor,
				                                 oldModule->GetVersion().patch,
				                                 newModule->GetVersion().major,
				                                 newModule->GetVersion().minor,
				                                 newModule->GetVersion().patch);
			}
			else
			{
				Dia::Core::Log::OutputVaradicLine("ERROR: HotReload FAILED for module %s: %s",
				                                 oldModuleId.AsChar(),
				                                 GetResultString(result));
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
				Dia::Core::Log::OutputVaradicLine("ERROR: HotReload - module %s does not allow hot reload",
				                                 context.oldModule->GetUniqueId().AsChar());
				return ReloadResult::kModuleNotReloadable;
			}

			// Check version compatibility
			if (!context.newModule->GetVersion().IsCompatibleWith(context.oldModule->GetVersion()))
			{
				Dia::Core::Log::OutputVaradicLine("ERROR: HotReload - version incompatible: old v%d.%d.%d, new v%d.%d.%d",
				                                 context.oldModule->GetVersion().major,
				                                 context.oldModule->GetVersion().minor,
				                                 context.oldModule->GetVersion().patch,
				                                 context.newModule->GetVersion().major,
				                                 context.newModule->GetVersion().minor,
				                                 context.newModule->GetVersion().patch);
				return ReloadResult::kVersionIncompatible;
			}

			// Check if dependent modules are stopped
			for (Module* dependent : context.dependentModules)
			{
				if (dependent->HasStarted())
				{
					Dia::Core::Log::OutputVaradicLine("ERROR: HotReload - dependent module %s is still running",
					                                 dependent->GetUniqueId().AsChar());
					return ReloadResult::kDependentModulesRunning;
				}
			}

			return ReloadResult::kSuccess;
		}

		//---------------------------------------------------------------------------------------------------------
		HotReloadManager::ReloadResult HotReloadManager::PerformReload(ReloadContext& context)
		{
			// Step 1: Stop old module if running
			if (context.wasRunning)
			{
				context.oldModule->Stop();
				Dia::Core::Log::OutputVaradicLine("HotReload: Stopped old module %s",
				                                 context.oldModule->GetUniqueId().AsChar());
			}

			// Step 2: Save state from old module
			context.savedState = context.oldModule->SaveState();
			if (context.savedState != nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("HotReload: Saved state from old module");
			}

			// Step 3: Update dependency references in dependent modules
			UpdateDependencyReferences(context.oldModule, context.newModule);

			// Step 4: Add new module to ProcessingUnit
			// Note: We don't remove old module yet in case we need to rollback
			mProcessingUnit->AddModule(context.newModule);

			// Step 5: Restore state to new module
			if (context.savedState != nullptr)
			{
				context.newModule->RestoreState(context.savedState);
				Dia::Core::Log::OutputVaradicLine("HotReload: Restored state to new module");
			}

			// Step 6: Start new module if old was running
			if (context.wasRunning)
			{
				ErrorInfo startError = context.newModule->DoStartWithError(nullptr);
				if (startError.IsFailure())
				{
					Dia::Core::Log::OutputVaradicLine("ERROR: HotReload - new module failed to start: %s",
					                                 startError.message ? startError.message : "Unknown error");

					// Rollback: restore old module
					UpdateDependencyReferences(context.newModule, context.oldModule);
					mProcessingUnit->AddModule(context.oldModule);

					if (context.wasRunning)
					{
						context.oldModule->Start();
					}

					return ReloadResult::kNewModuleStartFailed;
				}

				Dia::Core::Log::OutputVaradicLine("HotReload: Started new module %s",
				                                 context.newModule->GetUniqueId().AsChar());
			}

			// Step 7: Clean up old module
			// Note: In a production system, you might want to delay deletion or use a garbage collector
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
			// For now, log that this needs to be done manually or by derived classes
			Dia::Core::Log::OutputVaradicLine("HotReload: Updated dependency references from %s to %s",
			                                 oldModule->GetUniqueId().AsChar(),
			                                 newModule->GetUniqueId().AsChar());
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
