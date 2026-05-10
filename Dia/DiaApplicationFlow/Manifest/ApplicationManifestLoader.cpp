#include "ApplicationManifestLoader.h"
#include "JsonApplicationManifestSerializer.h"
#include "ManifestComposer.h"

#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <cstring>

namespace Dia
{
	namespace Application
	{
		//-----------------------------------------------------------------------------
		// ApplicationManifestLoader
		//-----------------------------------------------------------------------------

		ApplicationManifestLoader::ApplicationManifestLoader(ApplicationTypeRegistry& registry)
			: mRegistry(registry)
			, mErrors()
			, mValidator(registry)
		{
		}

		ApplicationManifestLoader::~ApplicationManifestLoader()
		{
		}

		ManifestValidationResult ApplicationManifestLoader::LoadFromFile(const char* filePath, ApplicationManifest& outManifest)
		{
			ClearErrors();
			ManifestComposer composer;
			ManifestValidationResult result = composer.ComposeSingleManifest(filePath, outManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				const auto& composerErrors = composer.GetErrors();
				for (unsigned int i = 0; i < composerErrors.Size(); ++i)
					mErrors.Add(composerErrors[i]);
				return result;
			}
			return Validate(outManifest);
		}

		ManifestValidationResult ApplicationManifestLoader::LoadFromString(const char* jsonString, ApplicationManifest& outManifest)
		{
			ClearErrors();
			JsonApplicationManifestSerializer serializer;
			auto result = serializer.Load(jsonString, outManifest);
			if (!result)
			{
				const char* err = result.error ? result.error : "parse error";
				ManifestValidationResult code = (strcmp(err, "json parse error") == 0)
					? ManifestValidationResult::kInvalidJSON
					: ManifestValidationResult::kMissingRequiredField;
				AddError(code, err, "json");
				return code;
			}
			return Validate(outManifest);
		}

		ManifestValidationResult ApplicationManifestLoader::Validate(const ApplicationManifest& manifest)
		{
			ClearErrors();

			// Delegate to ManifestValidator
			ManifestValidationResult result = mValidator.Validate(manifest);

			// Copy errors from validator
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& validatorErrors = mValidator.GetErrors();
			for (unsigned int i = 0; i < validatorErrors.Size(); ++i)
			{
				mErrors.Add(validatorErrors[i]);
			}

			return result;
		}

		ProcessingUnit* ApplicationManifestLoader::Instantiate(const ApplicationManifest::ProcessingUnitEntry& entry)
		{
			ClearErrors();

			ApplicationTypeRegistry& registry = mRegistry;

			// Create ProcessingUnit via registry
			Json::Value emptyConfig;
			if (entry.config != nullptr)
			{
				emptyConfig = *entry.config;
			}

			ProcessingUnit* pu = registry.CreateProcessingUnit(entry.typeId, entry.instanceId, emptyConfig);
			if (pu == nullptr)
			{
				Dia::Core::Containers::String256 msg;
				msg.Format("Failed to create ProcessingUnit of type '%s'", entry.typeId.AsChar());
				AddError(ManifestValidationResult::kUnknownType, msg.AsCStr(), "instantiate");
				return nullptr;
			}

			pu->mTypeRegistry = &mRegistry;

			// Set frequency and thread limiting
			if (entry.frequencyHz > 0.0f)
			{
				pu->EnableThreadLimiting(entry.frequencyHz);
			}
			else
			{
				pu->DisableThreadLimiting();
			}

			// Create all phases and add to PU
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Phase*, Dia::Core::StringCRCHashFunctor> PhaseMap;
			unsigned int phaseCount = entry.phases.Size() > 0 ? entry.phases.Size() : 1;
			PhaseMap phaseMap(phaseCount, phaseCount);

			for (unsigned int i = 0; i < entry.phases.Size(); ++i)
			{
				const ApplicationManifest::PhaseEntry& phaseEntry = entry.phases[i];

				Json::Value phaseConfig;
				if (phaseEntry.config != nullptr)
				{
					phaseConfig = *phaseEntry.config;
				}

				Phase* phase = registry.CreatePhase(phaseEntry.typeId, pu, phaseEntry.instanceId, phaseConfig);
				if (phase == nullptr)
				{
					Dia::Core::Containers::String256 msg;
					msg.Format("Failed to create Phase of type '%s'", phaseEntry.typeId.AsChar());
					AddError(ManifestValidationResult::kUnknownType, msg.AsCStr(), "instantiate");

					// Clean up and return (PU will auto-delete all owned phases/modules)
					delete pu;
					return nullptr;
				}

				phaseMap.Add(phaseEntry.instanceId, phase);
				pu->AddPhaseWithOwnership(Dia::Core::UniquePtr<Phase>(phase));
			}

			// Create all modules and add to PU
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> ModuleMap;
			unsigned int moduleCount = entry.modules.Size() > 0 ? entry.modules.Size() : 1;
			ModuleMap moduleMap(moduleCount, moduleCount);

			for (unsigned int i = 0; i < entry.modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& moduleEntry = entry.modules[i];

				Json::Value moduleConfig;
				if (moduleEntry.config != nullptr)
				{
					moduleConfig = *moduleEntry.config;
				}

				Module* module = registry.CreateModule(moduleEntry.typeId, pu, moduleEntry.instanceId, moduleConfig);
				if (module == nullptr)
				{
					Dia::Core::Containers::String256 msg;
					msg.Format("Failed to create Module of type '%s'", moduleEntry.typeId.AsChar());
					AddError(ManifestValidationResult::kUnknownType, msg.AsCStr(), "instantiate");

					// Clean up and return (PU will auto-delete all owned phases/modules)
					delete pu;
					return nullptr;
				}

				moduleMap.Add(moduleEntry.instanceId, module);
				pu->AddModuleWithOwnership(Dia::Core::UniquePtr<Module>(module));

				// Add module to all phases it belongs to
				for (unsigned int j = 0; j < moduleEntry.phaseIds.Size(); ++j)
				{
					const Dia::Core::StringCRC& phaseId = moduleEntry.phaseIds[j];
					Phase** phasePtr = phaseMap.TryGetItem(phaseId);
					if (phasePtr != nullptr && *phasePtr != nullptr)
					{
						(*phasePtr)->AddModule(module);
					}
					else
					{
						DIA_LOG_WARNING("Application", "Module '%s' references non-existent phase '%s'",
							moduleEntry.instanceId.AsChar(), phaseId.AsChar());
					}
				}
			}

			// Set up module dependencies
			for (unsigned int i = 0; i < entry.modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& moduleEntry = entry.modules[i];
				Module** modulePtr = moduleMap.TryGetItem(moduleEntry.instanceId);
				if (modulePtr == nullptr || *modulePtr == nullptr)
					continue;

				Module* module = *modulePtr;

				for (unsigned int j = 0; j < moduleEntry.dependencies.Size(); ++j)
				{
					const Dia::Core::StringCRC& depId = moduleEntry.dependencies[j];
					Module** depPtr = moduleMap.TryGetItem(depId);
					if (depPtr != nullptr && *depPtr != nullptr)
					{
						module->AddDependancy(*depPtr);
					}
					else
					{
						DIA_LOG_WARNING("Application", "Module '%s' depends on non-existent module '%s'",
							moduleEntry.instanceId.AsChar(), depId.AsChar());
					}
				}
			}

			// Set up phase transitions
			for (unsigned int i = 0; i < entry.transitions.Size(); ++i)
			{
				const ApplicationManifest::PhaseTransition& transition = entry.transitions[i];
				Phase** fromPhasePtr = phaseMap.TryGetItem(transition.fromPhase);
				Phase** toPhasePtr = phaseMap.TryGetItem(transition.toPhase);

				if (fromPhasePtr != nullptr && *fromPhasePtr != nullptr &&
					toPhasePtr != nullptr && *toPhasePtr != nullptr)
				{
					pu->AddPhaseTransiton(*fromPhasePtr, *toPhasePtr);
				}
				else
				{
					DIA_LOG_WARNING("Application", "Invalid phase transition from '%s' to '%s'",
						transition.fromPhase.AsChar(), transition.toPhase.AsChar());
				}
			}

			// Set initial phase
			Phase** initialPhasePtr = phaseMap.TryGetItem(entry.initialPhase);
			if (initialPhasePtr != nullptr && *initialPhasePtr != nullptr)
			{
				pu->SetInitialPhase(*initialPhasePtr);
			}
			else
			{
				DIA_LOG_WARNING("Application", "Invalid initial phase '%s'", entry.initialPhase.AsChar());
			}

			return pu;
		}

		ManifestValidationResult ApplicationManifestLoader::ReloadManifest(const char* filePath, ProcessingUnit* existingPU)
		{
			ClearErrors();

			// Validate input
			if (existingPU == nullptr)
			{
				AddError(ManifestValidationResult::kInvalidJSON, "ProcessingUnit pointer is null", "reload");
				return ManifestValidationResult::kInvalidJSON;
			}

			// 1. Load and validate new manifest
			ApplicationManifest newManifest;
			ManifestValidationResult result = LoadFromFile(filePath, newManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				// Errors already populated by LoadFromFile
				return result;
			}

			// Validate that we have exactly one ProcessingUnit entry (we only support reloading a single PU)
			if (newManifest.processingUnits.Size() != 1)
			{
				AddError(ManifestValidationResult::kInvalidJSON, "ReloadManifest requires exactly one processing_unit entry", "reload");
				return ManifestValidationResult::kInvalidJSON;
			}

			const ApplicationManifest::ProcessingUnitEntry& newPUEntry = newManifest.processingUnits[0];

			// 2. Build maps of current modules and phases for comparison
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> ModuleMap;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Phase*, Dia::Core::StringCRCHashFunctor> PhaseMap;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> IdSet;

			// Get current modules and phases from ProcessingUnit
			// NOTE: We access private members via friendship (ProcessingUnit is a friend of this loader)
			ProcessingUnit::ModuleTable& currentModules = existingPU->mAssociatedModules;
			ProcessingUnit::PhasesTable& currentPhases = existingPU->mAssociatedPhases;

			// Build sets of new module/phase IDs
			unsigned int modIdCount = newPUEntry.modules.Size() > 0 ? newPUEntry.modules.Size() : 1;
			unsigned int phsIdCount = newPUEntry.phases.Size() > 0 ? newPUEntry.phases.Size() : 1;
			IdSet newModuleIds(modIdCount, modIdCount);
			IdSet newPhaseIds(phsIdCount, phsIdCount);

			for (unsigned int i = 0; i < newPUEntry.modules.Size(); ++i)
			{
				newModuleIds.Add(newPUEntry.modules[i].instanceId, true);
			}

			for (unsigned int i = 0; i < newPUEntry.phases.Size(); ++i)
			{
				newPhaseIds.Add(newPUEntry.phases[i].instanceId, true);
			}

			// 3. Identify modules to remove (in current but not in new)
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> modulesToRemove;
			for (auto it = currentModules.Begin(); it != currentModules.End(); ++it)
			{
				const Dia::Core::StringCRC& moduleId = it.Key();
				if (!newModuleIds.ContainsKey(moduleId))
				{
					modulesToRemove.Add(moduleId);
				}
			}

			// 4. Remove modules no longer present
			// TODO: We should check if modules are in use before removing (safe shutdown)
			for (unsigned int i = 0; i < modulesToRemove.Size(); ++i)
			{
				const Dia::Core::StringCRC& moduleId = modulesToRemove[i];
				Module** modulePtr = currentModules.TryGetItem(moduleId);
				if (modulePtr != nullptr && *modulePtr != nullptr)
				{
					Module* module = *modulePtr;
					// Stop module if running
					if (module->GetState() == StateObject::StateEnum::kRunning)
					{
						module->Stop();
					}

					// Remove from ProcessingUnit
					existingPU->RemoveModule(moduleId);

					DIA_LOG_INFO("Application", "Hot reload: Removed module '%s'", moduleId.AsChar());
				}
			}

			// 5. Update existing modules with new configuration
			ApplicationTypeRegistry& registry = mRegistry;
			for (unsigned int i = 0; i < newPUEntry.modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& moduleEntry = newPUEntry.modules[i];
				Module** existingModulePtr = currentModules.TryGetItem(moduleEntry.instanceId);

				if (existingModulePtr != nullptr && *existingModulePtr != nullptr)
				{
					Module* existingModule = *existingModulePtr;
					// Module already exists, update its configuration
					if (moduleEntry.config != nullptr)
					{
						bool configSuccess = existingModule->DeserializeConfig(*moduleEntry.config);
						if (!configSuccess)
						{
							DIA_LOG_WARNING("Application", "Failed to update config for module '%s' during hot reload", moduleEntry.instanceId.AsChar());
						}
						else
						{
							DIA_LOG_INFO("Application", "Hot reload: Updated config for module '%s'", moduleEntry.instanceId.AsChar());
						}
					}
				}
			}

			// 6. Add new modules
			// NOTE: This is a simplified implementation. Full implementation would:
			//   - Use HotReloadManager for module state preservation
			//   - Handle module dependencies correctly
			//   - Rebuild phase transitions properly
			//   - Queue phase transition to safe state before changes
			for (unsigned int i = 0; i < newPUEntry.modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& moduleEntry = newPUEntry.modules[i];

				// Check if module already exists
				if (currentModules.ContainsKey(moduleEntry.instanceId))
				{
					continue; // Already handled above
				}

				// Create new module
				Json::Value moduleConfig;
				if (moduleEntry.config != nullptr)
				{
					moduleConfig = *moduleEntry.config;
				}

				Module* newModule = registry.CreateModule(moduleEntry.typeId, existingPU, moduleEntry.instanceId, moduleConfig);
				if (newModule == nullptr)
				{
					DIA_LOG_ERROR("Application", "Hot reload: Failed to create module '%s' (type '%s') — skipping", moduleEntry.instanceId.AsChar(), moduleEntry.typeId.AsChar());
					Dia::Core::Containers::String256 msg;
					msg.Format("Failed to create new module '%s' during hot reload", moduleEntry.instanceId.AsChar());
					AddError(ManifestValidationResult::kUnknownType, msg.AsCStr(), "reload");
					continue;
				}

				// Add to ProcessingUnit with ownership
				existingPU->AddModuleWithOwnership(Dia::Core::UniquePtr<Module>(newModule));

				DIA_LOG_INFO("Application", "Hot reload: Added new module '%s'", moduleEntry.instanceId.AsChar());
			}

			// 7. TODO: Handle phases
			// NOTE: Phase hot-reloading is more complex and should be implemented when needed
			//   - Need to handle phase transitions carefully
			//   - Need to manage module-phase relationships
			//   - May require pausing/transitioning to safe state
			// For now, log a warning if phases have changed
			if (newPUEntry.phases.Size() != currentPhases.Size())
			{
				DIA_LOG_WARNING("Application", "Phase count changed during hot reload, but phase reloading is not yet implemented");
				DIA_LOG_WARNING("Application", "Current phases: %u, New phases: %u", currentPhases.Size(), newPUEntry.phases.Size());
			}

			// 8. TODO: Rebuild phase transitions
			// NOTE: Transition reloading should be implemented when phase reloading is added

			// 9. Update ProcessingUnit frequency settings
			if (newPUEntry.frequencyHz > 0.0f)
			{
				existingPU->EnableThreadLimiting(newPUEntry.frequencyHz);
			}
			else
			{
				existingPU->DisableThreadLimiting();
			}

			DIA_LOG_INFO("Application", "Hot reload completed successfully from '%s'", filePath);
			return ManifestValidationResult::kSuccess;
		}

		ManifestValidationResult ApplicationManifestLoader::ComposeManifests(
			const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
			ApplicationManifest& outComposedManifest)
		{
			ClearErrors();

			if (filePaths.Size() == 0)
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "No manifest files provided", "compose");
				return ManifestValidationResult::kMissingRequiredField;
			}

			ManifestComposer composer;
			ManifestValidationResult result = composer.ComposeManifests(filePaths, outComposedManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				const auto& composerErrors = composer.GetErrors();
				for (unsigned int i = 0; i < composerErrors.Size(); ++i)
					mErrors.Add(composerErrors[i]);
				return result;
			}
			return Validate(outComposedManifest);
		}

		const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& ApplicationManifestLoader::GetErrors() const
		{
			return mErrors;
		}

		void ApplicationManifestLoader::ClearErrors()
		{
			mErrors.RemoveAll();
			mValidator.ClearErrors();
		}


		void ApplicationManifestLoader::AddError(ManifestValidationResult code, const char* message, const char* context)
		{
			mErrors.Add(ManifestValidationError(code, message, context));
		}
	}
}
