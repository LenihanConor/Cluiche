////////////////////////////////////////////////////////////////////////////////
// Filename: HotReloadManager.h
//
// Hot reload manager for runtime module replacement.
// Allows swapping modules without restarting the application.
//
// Usage:
//   1. Create new version of module
//   2. Call hotReloadManager.ReplaceModule(oldModuleId, newModule)
//   3. Manager handles stop/state-transfer/start/cleanup
//
// Requirements:
//   - Module must return true from CanHotReload()
//   - New module must be version-compatible (same major version)
//   - Module should implement SaveState/RestoreState for state transfer
//
////////////////////////////////////////////////////////////////////////////////
#ifndef _HOTRELOADMANAGER_H_
#define _HOTRELOADMANAGER_H_

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include <vector>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class Phase;

		////////////////////////////////////////////////////////////////////////////////
		// Class name: HotReloadManager
		// Description: Manages runtime module replacement (hot reload)
		////////////////////////////////////////////////////////////////////////////////
		class HotReloadManager
		{
		public:
			////////////////////////////////////////////////////////////////////////////////
			// Enum name: ReloadResult
			// Description: Result codes for module replacement operations
			////////////////////////////////////////////////////////////////////////////////
			enum class ReloadResult
			{
				kSuccess,                 // Module replaced successfully
				kModuleNotFound,          // Old module ID not found in ProcessingUnit
				kModuleNotReloadable,     // Module's CanHotReload() returned false
				kVersionIncompatible,     // New module version incompatible with old
				kModuleRunning,           // Module is running (must be stopped first)
				kDependentModulesRunning, // Modules that depend on this one are still running
				kNewModuleStartFailed,    // New module failed to start
				kInvalidProcessingUnit    // ProcessingUnit pointer is null
			};

			HotReloadManager(ProcessingUnit* processingUnit);

			/**
			 * @brief Replace a module in all phases that use it
			 * @param oldModuleId CRC ID of module to replace
			 * @param newModule Pointer to new module instance. Ownership transfers to ProcessingUnit.
			 * @return Result code indicating success or failure reason
			 *
			 * Process:
			 *  1. Validate old module exists and is reloadable
			 *  2. Check version compatibility
			 *  3. Stop old module (if running)
			 *  4. Save state from old module
			 *  5. Replace in all phases
			 *  6. Update dependent modules' references
			 *  7. Restore state to new module
			 *  8. Start new module (if old was running)
			 *  9. Delete old module
			 */
			ReloadResult ReplaceModule(const Dia::Core::StringCRC& oldModuleId,
			                          Module* newModule);

			/**
			 * @brief Replace a module in a specific phase only
			 * @param phaseId CRC ID of phase containing the module
			 * @param oldModuleId CRC ID of module to replace
			 * @param newModule Pointer to new module instance
			 * @return Result code indicating success or failure reason
			 */
			ReloadResult ReplaceModuleInPhase(const Dia::Core::StringCRC& phaseId,
			                                  const Dia::Core::StringCRC& oldModuleId,
			                                  Module* newModule);

			/**
			 * @brief Get human-readable string for reload result
			 */
			static const char* GetResultString(ReloadResult result);

		private:
			ProcessingUnit* mProcessingUnit;

			// Context for a reload operation
			struct ReloadContext
			{
				Module* oldModule;
				Module* newModule;
				void* savedState;
				bool wasRunning;
				std::vector<Module*> dependentModules;  // Modules that depend on this one
			};

			// Validation and execution steps
			ReloadResult ValidateReload(const ReloadContext& context);
			ReloadResult PerformReload(ReloadContext& context);
			void CollectDependentModules(Module* module, std::vector<Module*>& dependents);
			void UpdateDependencyReferences(Module* oldModule, Module* newModule);
		};
	}
}

#endif // _HOTRELOADMANAGER_H_
