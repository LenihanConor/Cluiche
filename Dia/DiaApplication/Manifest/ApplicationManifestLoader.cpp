#include "ApplicationManifestLoader.h"

#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Core/Log.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Memory/UniquePtr.h>

#include <fstream>
#include <sstream>

namespace Dia
{
	namespace Application
	{
		//-----------------------------------------------------------------------------
		// ApplicationManifestLoader
		//-----------------------------------------------------------------------------

		ApplicationManifestLoader::ApplicationManifestLoader()
			: mErrors()
		{
		}

		ApplicationManifestLoader::~ApplicationManifestLoader()
		{
		}

		ManifestValidationResult ApplicationManifestLoader::LoadFromFile(const char* filePath, ApplicationManifest& outManifest)
		{
			ClearErrors();

			// Read file contents
			std::ifstream fileStream(filePath, std::ios::in | std::ios::binary);
			if (!fileStream.is_open())
			{
				Dia::Core::Containers::String256 msg;
				msg.Format("Failed to open file: %s", filePath);
				AddError(ManifestValidationResult::kImportNotFound, msg.AsCStr(), "file");
				return ManifestValidationResult::kImportNotFound;
			}

			// Read entire file into string
			std::stringstream buffer;
			buffer << fileStream.rdbuf();
			fileStream.close();

			// Parse JSON
			return LoadFromString(buffer.str().c_str(), outManifest);
		}

		ManifestValidationResult ApplicationManifestLoader::LoadFromString(const char* jsonString, ApplicationManifest& outManifest)
		{
			ClearErrors();

			// Parse JSON string
			Json::Value root;
			Json::Reader reader;
			bool parsingSuccessful = reader.parse(jsonString, root, false);

			if (!parsingSuccessful)
			{
				Dia::Core::Containers::String256 msg;
				msg.Format("JSON parsing failed: %s", reader.getFormattedErrorMessages().c_str());
				AddError(ManifestValidationResult::kInvalidJSON, msg.AsCStr(), "json");
				return ManifestValidationResult::kInvalidJSON;
			}

			// Parse into manifest structure
			if (!ParseJSON(root, outManifest))
			{
				return ManifestValidationResult::kInvalidJSON;
			}

			// Validate the manifest
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

			ApplicationTypeRegistry& registry = ApplicationTypeRegistry::Instance();

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
			PhaseMap phaseMap(entry.phases.Size(), entry.phases.Size());

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
			ModuleMap moduleMap(entry.modules.Size(), entry.modules.Size());

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
						Dia::Core::Log::OutputVaradicLine("Warning: Module '%s' references non-existent phase '%s'",
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
						Dia::Core::Log::OutputVaradicLine("Warning: Module '%s' depends on non-existent module '%s'",
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
					Dia::Core::Log::OutputVaradicLine("Warning: Invalid phase transition from '%s' to '%s'",
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
				Dia::Core::Log::OutputVaradicLine("Warning: Invalid initial phase '%s'", entry.initialPhase.AsChar());
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
			IdSet newModuleIds(newPUEntry.modules.Size(), newPUEntry.modules.Size());
			IdSet newPhaseIds(newPUEntry.phases.Size(), newPUEntry.phases.Size());

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

					Dia::Core::Log::OutputVaradicLine("Hot reload: Removed module '%s'", moduleId.AsChar());
				}
			}

			// 5. Update existing modules with new configuration
			ApplicationTypeRegistry& registry = ApplicationTypeRegistry::Instance();
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
							Dia::Core::Log::OutputVaradicLine("Warning: Failed to update config for module '%s' during hot reload", moduleEntry.instanceId.AsChar());
						}
						else
						{
							Dia::Core::Log::OutputVaradicLine("Hot reload: Updated config for module '%s'", moduleEntry.instanceId.AsChar());
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
					Dia::Core::Containers::String256 msg;
					msg.Format("Failed to create new module '%s' during hot reload", moduleEntry.instanceId.AsChar());
					AddError(ManifestValidationResult::kUnknownType, msg.AsCStr(), "reload");
					continue; // Skip this module but continue with others
				}

				// Add to ProcessingUnit with ownership
				existingPU->AddModuleWithOwnership(Dia::Core::UniquePtr<Module>(newModule));

				Dia::Core::Log::OutputVaradicLine("Hot reload: Added new module '%s'", moduleEntry.instanceId.AsChar());
			}

			// 7. TODO: Handle phases
			// NOTE: Phase hot-reloading is more complex and should be implemented when needed
			//   - Need to handle phase transitions carefully
			//   - Need to manage module-phase relationships
			//   - May require pausing/transitioning to safe state
			// For now, log a warning if phases have changed
			if (newPUEntry.phases.Size() != currentPhases.Size())
			{
				Dia::Core::Log::OutputVaradicLine("Warning: Phase count changed during hot reload, but phase reloading is not yet implemented");
				Dia::Core::Log::OutputVaradicLine("         Current phases: %u, New phases: %u", currentPhases.Size(), newPUEntry.phases.Size());
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

			Dia::Core::Log::OutputVaradicLine("Hot reload completed successfully from '%s'", filePath);
			return ManifestValidationResult::kSuccess;
		}

		ManifestValidationResult ApplicationManifestLoader::ComposeManifests(
			const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
			ApplicationManifest& outComposedManifest)
		{
			ClearErrors();

			// TODO: Delegate to ManifestComposer (Task #7)
			// For now, just load the first manifest
			if (filePaths.Size() > 0)
			{
				return LoadFromFile(filePaths[0], outComposedManifest);
			}

			AddError(ManifestValidationResult::kMissingRequiredField, "No manifest files provided", "compose");
			return ManifestValidationResult::kMissingRequiredField;
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

		//-----------------------------------------------------------------------------
		// Private Helper Methods
		//-----------------------------------------------------------------------------

		bool ApplicationManifestLoader::ParseJSON(const Json::Value& root, ApplicationManifest& outManifest)
		{
			// Parse version (required)
			if (!root.isMember("version"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'version' field", "manifest");
				return false;
			}
			outManifest.version = root["version"].asUInt();

			// Parse processing_units (required)
			if (!root.isMember("processing_units"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'processing_units' field", "manifest");
				return false;
			}

			const Json::Value& processingUnits = root["processing_units"];
			if (!processingUnits.isArray())
			{
				AddError(ManifestValidationResult::kInvalidJSON, "'processing_units' must be an array", "manifest");
				return false;
			}

			for (unsigned int i = 0; i < processingUnits.size(); ++i)
			{
				ApplicationManifest::ProcessingUnitEntry entry;
				if (!ParseProcessingUnit(processingUnits[i], entry))
				{
					return false;
				}
				outManifest.processingUnits.Add(entry);
			}

			// Parse imports (optional)
			if (root.isMember("imports"))
			{
				const Json::Value& imports = root["imports"];
				if (imports.isArray())
				{
					for (unsigned int i = 0; i < imports.size(); ++i)
					{
						if (imports[i].isString())
						{
							// Note: String is copied here, need to manage memory
							const char* importPath = imports[i].asCString();
							outManifest.imports.Add(importPath);
						}
					}
				}
			}

			// Parse metadata (optional)
			if (root.isMember("metadata"))
			{
				outManifest.metadata = new Json::Value(root["metadata"]);
			}

			return true;
		}

		bool ApplicationManifestLoader::ParseProcessingUnit(const Json::Value& puJson, ApplicationManifest::ProcessingUnitEntry& outEntry)
		{
			// Parse type_id (required)
			if (!puJson.isMember("type_id"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'type_id' in processing_unit", "processing_unit");
				return false;
			}
			outEntry.typeId = Dia::Core::StringCRC(puJson["type_id"].asCString());

			// Parse instance_id (required)
			if (!puJson.isMember("instance_id"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'instance_id' in processing_unit", "processing_unit");
				return false;
			}
			outEntry.instanceId = Dia::Core::StringCRC(puJson["instance_id"].asCString());

			// Parse frequency_hz (optional, default -1 = unlimited)
			outEntry.frequencyHz = puJson.get("frequency_hz", -1.0f).asFloat();

			// Parse dedicated_thread (optional, default false)
			outEntry.dedicatedThread = puJson.get("dedicated_thread", false).asBool();

			// Parse config (optional)
			if (puJson.isMember("config"))
			{
				outEntry.config = new Json::Value(puJson["config"]);
			}

			// Parse phases (required)
			if (!puJson.isMember("phases"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'phases' in processing_unit", "processing_unit");
				return false;
			}

			const Json::Value& phases = puJson["phases"];
			if (!phases.isArray())
			{
				AddError(ManifestValidationResult::kInvalidJSON, "'phases' must be an array", "processing_unit");
				return false;
			}

			for (unsigned int i = 0; i < phases.size(); ++i)
			{
				ApplicationManifest::PhaseEntry phaseEntry;
				if (!ParsePhase(phases[i], phaseEntry))
				{
					return false;
				}
				outEntry.phases.Add(phaseEntry);
			}

			// Parse transitions (required)
			if (!puJson.isMember("transitions"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'transitions' in processing_unit", "processing_unit");
				return false;
			}

			const Json::Value& transitions = puJson["transitions"];
			if (!transitions.isArray())
			{
				AddError(ManifestValidationResult::kInvalidJSON, "'transitions' must be an array", "processing_unit");
				return false;
			}

			for (unsigned int i = 0; i < transitions.size(); ++i)
			{
				ApplicationManifest::PhaseTransition transition;
				if (!ParsePhaseTransition(transitions[i], transition))
				{
					return false;
				}
				outEntry.transitions.Add(transition);
			}

			// Parse initial_phase (required)
			if (!puJson.isMember("initial_phase"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'initial_phase' in processing_unit", "processing_unit");
				return false;
			}
			outEntry.initialPhase = Dia::Core::StringCRC(puJson["initial_phase"].asCString());

			// Parse modules (required)
			if (!puJson.isMember("modules"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'modules' in processing_unit", "processing_unit");
				return false;
			}

			const Json::Value& modules = puJson["modules"];
			if (!modules.isArray())
			{
				AddError(ManifestValidationResult::kInvalidJSON, "'modules' must be an array", "processing_unit");
				return false;
			}

			for (unsigned int i = 0; i < modules.size(); ++i)
			{
				ApplicationManifest::ModuleEntry moduleEntry;
				if (!ParseModule(modules[i], moduleEntry))
				{
					return false;
				}
				outEntry.modules.Add(moduleEntry);
			}

			return true;
		}

		bool ApplicationManifestLoader::ParsePhase(const Json::Value& phaseJson, ApplicationManifest::PhaseEntry& outEntry)
		{
			// Parse type_id (required)
			if (!phaseJson.isMember("type_id"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'type_id' in phase", "phase");
				return false;
			}
			outEntry.typeId = Dia::Core::StringCRC(phaseJson["type_id"].asCString());

			// Parse instance_id (required)
			if (!phaseJson.isMember("instance_id"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'instance_id' in phase", "phase");
				return false;
			}
			outEntry.instanceId = Dia::Core::StringCRC(phaseJson["instance_id"].asCString());

			// Parse config (optional)
			if (phaseJson.isMember("config"))
			{
				outEntry.config = new Json::Value(phaseJson["config"]);
			}

			return true;
		}

		bool ApplicationManifestLoader::ParseModule(const Json::Value& moduleJson, ApplicationManifest::ModuleEntry& outEntry)
		{
			// Parse type_id (required)
			if (!moduleJson.isMember("type_id"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'type_id' in module", "module");
				return false;
			}
			outEntry.typeId = Dia::Core::StringCRC(moduleJson["type_id"].asCString());

			// Parse instance_id (required)
			if (!moduleJson.isMember("instance_id"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'instance_id' in module", "module");
				return false;
			}
			outEntry.instanceId = Dia::Core::StringCRC(moduleJson["instance_id"].asCString());

			// Parse phase_ids (required)
			if (!moduleJson.isMember("phase_ids"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'phase_ids' in module", "module");
				return false;
			}

			const Json::Value& phaseIds = moduleJson["phase_ids"];
			if (!phaseIds.isArray())
			{
				AddError(ManifestValidationResult::kInvalidJSON, "'phase_ids' must be an array", "module");
				return false;
			}

			for (unsigned int i = 0; i < phaseIds.size(); ++i)
			{
				if (phaseIds[i].isString())
				{
					outEntry.phaseIds.Add(Dia::Core::StringCRC(phaseIds[i].asCString()));
				}
			}

			// Parse dependencies (optional)
			if (moduleJson.isMember("dependencies"))
			{
				const Json::Value& dependencies = moduleJson["dependencies"];
				if (dependencies.isArray())
				{
					for (unsigned int i = 0; i < dependencies.size(); ++i)
					{
						if (dependencies[i].isString())
						{
							outEntry.dependencies.Add(Dia::Core::StringCRC(dependencies[i].asCString()));
						}
					}
				}
			}

			// Parse config (optional)
			if (moduleJson.isMember("config"))
			{
				outEntry.config = new Json::Value(moduleJson["config"]);
			}

			return true;
		}

		bool ApplicationManifestLoader::ParsePhaseTransition(const Json::Value& transitionJson, ApplicationManifest::PhaseTransition& outTransition)
		{
			// Parse from (required)
			if (!transitionJson.isMember("from"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'from' in transition", "transition");
				return false;
			}
			outTransition.fromPhase = Dia::Core::StringCRC(transitionJson["from"].asCString());

			// Parse to (required)
			if (!transitionJson.isMember("to"))
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Missing 'to' in transition", "transition");
				return false;
			}
			outTransition.toPhase = Dia::Core::StringCRC(transitionJson["to"].asCString());

			return true;
		}

		void ApplicationManifestLoader::AddError(ManifestValidationResult code, const char* message, const char* context)
		{
			mErrors.Add(ManifestValidationError(code, message, context));
		}
	}
}
