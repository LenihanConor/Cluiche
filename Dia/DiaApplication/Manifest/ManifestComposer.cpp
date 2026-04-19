#include "ManifestComposer.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Strings/stringutils.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Application
	{
		//-----------------------------------------------------------------------------
		// ManifestComposer
		//-----------------------------------------------------------------------------

		ManifestComposer::ManifestComposer()
			: mErrors()
			, mImportStack()
			, mProcessedFiles(64, 64)
		{
		}

		ManifestValidationResult ManifestComposer::ComposeManifests(
			const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
			ApplicationManifest& outComposedManifest)
		{
			ClearErrors();
			mImportStack.RemoveAll();
			mProcessedFiles.Clear();

			// Compose each file in order, merging into output manifest
			for (unsigned int i = 0; i < filePaths.Size(); ++i)
			{
				ApplicationManifest tempManifest;
				ManifestValidationResult result = ResolveImportsRecursive(filePaths[i], tempManifest);

				if (result != ManifestValidationResult::kSuccess)
				{
					return result;
				}

				// Merge into output
				MergeManifests(tempManifest, outComposedManifest);
			}

			return ManifestValidationResult::kSuccess;
		}

		ManifestValidationResult ManifestComposer::ComposeSingleManifest(
			const char* filePath,
			ApplicationManifest& outComposedManifest)
		{
			ClearErrors();
			mImportStack.RemoveAll();
			mProcessedFiles.Clear();

			return ResolveImportsRecursive(filePath, outComposedManifest);
		}

		ManifestValidationResult ManifestComposer::ResolveImportsRecursive(
			const char* filePath,
			ApplicationManifest& outManifest)
		{
			// Check for import cycle
			if (DetectImportCycle(filePath))
			{
				Dia::Core::Containers::String512 cyclePath;
				for (unsigned int i = 0; i < mImportStack.Size(); ++i)
				{
					if (i > 0)
					{
						cyclePath.Append(" -> ");
					}
					cyclePath.Append(mImportStack[i]);
				}
				cyclePath.Append(" -> ");
				cyclePath.Append(filePath);

				Dia::Core::Containers::String512 msg;
				msg.Format("Import cycle detected: %s", cyclePath.AsCStr());
				AddError(ManifestValidationResult::kImportCycle, msg.AsCStr(), filePath);
				return ManifestValidationResult::kImportCycle;
			}

			// Check if already processed (avoid reprocessing same file)
			Dia::Core::StringCRC filePathCRC(filePath);
			if (mProcessedFiles.ContainsKey(filePathCRC))
			{
				return ManifestValidationResult::kSuccess; // Already processed
			}

			// Load manifest from file
			ApplicationManifest currentManifest;
			ManifestValidationResult result = LoadManifestFromFile(filePath, currentManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				return result;
			}

			// Mark as being processed (for cycle detection)
			mImportStack.Add(filePath);

			// Recursively resolve imports (depth-first)
			for (unsigned int i = 0; i < currentManifest.imports.Size(); ++i)
			{
				const char* importPath = currentManifest.imports[i];
				ApplicationManifest importedManifest;

				result = ResolveImportsRecursive(importPath, importedManifest);
				if (result != ManifestValidationResult::kSuccess)
				{
					return result;
				}

				// Merge imported manifest into output
				MergeManifests(importedManifest, outManifest);
			}

			// Pop from stack
			mImportStack.RemoveAt(mImportStack.Size() - 1);

			// Mark as fully processed
			mProcessedFiles.Add(filePathCRC, true);

			// Merge current manifest into output (after imports, so it takes precedence)
			MergeManifests(currentManifest, outManifest);

			return ManifestValidationResult::kSuccess;
		}

		bool ManifestComposer::DetectImportCycle(const char* filePath)
		{
			// Check if filePath is already in the import stack
			for (unsigned int i = 0; i < mImportStack.Size(); ++i)
			{
				if (strcmp(mImportStack[i], filePath) == 0)
				{
					return true;
				}
			}
			return false;
		}

		void ManifestComposer::MergeManifests(const ApplicationManifest& source, ApplicationManifest& target)
		{
			// Use source version if target is uninitialized
			if (target.version == 0)
			{
				target.version = source.version;
			}

			// AC12 Rule 1: Processing units merged by instance_id (later overrides earlier)
			for (unsigned int i = 0; i < source.processingUnits.Size(); ++i)
			{
				const ApplicationManifest::ProcessingUnitEntry& sourcePU = source.processingUnits[i];

				// Find matching processing unit in target
				bool found = false;
				for (unsigned int j = 0; j < target.processingUnits.Size(); ++j)
				{
					ApplicationManifest::ProcessingUnitEntry& targetPU = target.processingUnits[j];

					if (targetPU.instanceId == sourcePU.instanceId)
					{
						// Merge into existing processing unit
						MergeProcessingUnit(sourcePU, targetPU);
						found = true;
						break;
					}
				}

				if (!found)
				{
					// Add new processing unit
					ApplicationManifest::ProcessingUnitEntry newPU;
					newPU.typeId = sourcePU.typeId;
					newPU.instanceId = sourcePU.instanceId;
					newPU.frequencyHz = sourcePU.frequencyHz;
					newPU.dedicatedThread = sourcePU.dedicatedThread;
					newPU.initialPhase = sourcePU.initialPhase;

					// Deep copy config
					if (sourcePU.config != nullptr)
					{
						newPU.config = new Json::Value(*sourcePU.config);
					}

					// Copy phases
					for (unsigned int k = 0; k < sourcePU.phases.Size(); ++k)
					{
						const ApplicationManifest::PhaseEntry& sourcePhase = sourcePU.phases[k];
						ApplicationManifest::PhaseEntry newPhase;
						newPhase.typeId = sourcePhase.typeId;
						newPhase.instanceId = sourcePhase.instanceId;
						if (sourcePhase.config != nullptr)
						{
							newPhase.config = new Json::Value(*sourcePhase.config);
						}
						newPU.phases.Add(newPhase);
					}

					// Copy modules
					for (unsigned int k = 0; k < sourcePU.modules.Size(); ++k)
					{
						const ApplicationManifest::ModuleEntry& sourceModule = sourcePU.modules[k];
						ApplicationManifest::ModuleEntry newModule;
						newModule.typeId = sourceModule.typeId;
						newModule.instanceId = sourceModule.instanceId;
						if (sourceModule.config != nullptr)
						{
							newModule.config = new Json::Value(*sourceModule.config);
						}

						// Copy phase IDs
						for (unsigned int m = 0; m < sourceModule.phaseIds.Size(); ++m)
						{
							newModule.phaseIds.Add(sourceModule.phaseIds[m]);
						}

						// Copy dependencies
						for (unsigned int m = 0; m < sourceModule.dependencies.Size(); ++m)
						{
							newModule.dependencies.Add(sourceModule.dependencies[m]);
						}

						newPU.modules.Add(newModule);
					}

					// Copy transitions
					for (unsigned int k = 0; k < sourcePU.transitions.Size(); ++k)
					{
						newPU.transitions.Add(sourcePU.transitions[k]);
					}

					target.processingUnits.Add(newPU);
				}
			}

			// Merge metadata (deep merge)
			if (source.metadata != nullptr)
			{
				if (target.metadata == nullptr)
				{
					target.metadata = new Json::Value(*source.metadata);
				}
				else
				{
					DeepMergeConfig(*source.metadata, *target.metadata);
				}
			}
		}

		void ManifestComposer::MergeProcessingUnit(
			const ApplicationManifest::ProcessingUnitEntry& source,
			ApplicationManifest::ProcessingUnitEntry& target)
		{
			// Override scalar fields (later wins)
			target.typeId = source.typeId;
			target.frequencyHz = source.frequencyHz;
			target.dedicatedThread = source.dedicatedThread;
			target.initialPhase = source.initialPhase;

			// Deep merge config
			if (source.config != nullptr)
			{
				if (target.config == nullptr)
				{
					target.config = new Json::Value(*source.config);
				}
				else
				{
					DeepMergeConfig(*source.config, *target.config);
				}
			}

			// AC12 Rule 2: Phases added if new instance_id, merged if existing (config overridden)
			MergePhases(source.phases, target.phases);

			// AC12 Rule 3: Modules added if new instance_id, merged if existing (config deep-merged)
			MergeModules(source.modules, target.modules);

			// AC12 Rule 4: Phase transitions are union (duplicates ignored)
			MergeTransitions(source.transitions, target.transitions);
		}

		void ManifestComposer::MergePhases(
			const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseEntry, 16>& sourcePhases,
			Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseEntry, 16>& targetPhases)
		{
			for (unsigned int i = 0; i < sourcePhases.Size(); ++i)
			{
				const ApplicationManifest::PhaseEntry& sourcePhase = sourcePhases[i];

				// Find matching phase in target
				bool found = false;
				for (unsigned int j = 0; j < targetPhases.Size(); ++j)
				{
					ApplicationManifest::PhaseEntry& targetPhase = targetPhases[j];

					if (targetPhase.instanceId == sourcePhase.instanceId)
					{
						// AC12 Rule 2: Config overridden (not deep merged for phases)
						targetPhase.typeId = sourcePhase.typeId;

						delete targetPhase.config;
						targetPhase.config = nullptr;

						if (sourcePhase.config != nullptr)
						{
							targetPhase.config = new Json::Value(*sourcePhase.config);
						}

						found = true;
						break;
					}
				}

				if (!found)
				{
					// Add new phase
					ApplicationManifest::PhaseEntry newPhase;
					newPhase.typeId = sourcePhase.typeId;
					newPhase.instanceId = sourcePhase.instanceId;
					if (sourcePhase.config != nullptr)
					{
						newPhase.config = new Json::Value(*sourcePhase.config);
					}
					targetPhases.Add(newPhase);
				}
			}
		}

		void ManifestComposer::MergeModules(
			const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::ModuleEntry, 32>& sourceModules,
			Dia::Core::Containers::DynamicArrayC<ApplicationManifest::ModuleEntry, 32>& targetModules)
		{
			for (unsigned int i = 0; i < sourceModules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& sourceModule = sourceModules[i];

				// Find matching module in target
				bool found = false;
				for (unsigned int j = 0; j < targetModules.Size(); ++j)
				{
					ApplicationManifest::ModuleEntry& targetModule = targetModules[j];

					if (targetModule.instanceId == sourceModule.instanceId)
					{
						// AC12 Rule 3: Config deep-merged for modules
						targetModule.typeId = sourceModule.typeId;

						if (sourceModule.config != nullptr)
						{
							if (targetModule.config == nullptr)
							{
								targetModule.config = new Json::Value(*sourceModule.config);
							}
							else
							{
								DeepMergeConfig(*sourceModule.config, *targetModule.config);
							}
						}

						// Merge phase IDs (union)
						for (unsigned int k = 0; k < sourceModule.phaseIds.Size(); ++k)
						{
							const Dia::Core::StringCRC& phaseId = sourceModule.phaseIds[k];

							// Check if already present
							bool phaseFound = false;
							for (unsigned int m = 0; m < targetModule.phaseIds.Size(); ++m)
							{
								if (targetModule.phaseIds[m] == phaseId)
								{
									phaseFound = true;
									break;
								}
							}

							if (!phaseFound)
							{
								targetModule.phaseIds.Add(phaseId);
							}
						}

						// Merge dependencies (union)
						for (unsigned int k = 0; k < sourceModule.dependencies.Size(); ++k)
						{
							const Dia::Core::StringCRC& depId = sourceModule.dependencies[k];

							// Check if already present
							bool depFound = false;
							for (unsigned int m = 0; m < targetModule.dependencies.Size(); ++m)
							{
								if (targetModule.dependencies[m] == depId)
								{
									depFound = true;
									break;
								}
							}

							if (!depFound)
							{
								targetModule.dependencies.Add(depId);
							}
						}

						found = true;
						break;
					}
				}

				if (!found)
				{
					// Add new module
					ApplicationManifest::ModuleEntry newModule;
					newModule.typeId = sourceModule.typeId;
					newModule.instanceId = sourceModule.instanceId;
					if (sourceModule.config != nullptr)
					{
						newModule.config = new Json::Value(*sourceModule.config);
					}

					// Copy phase IDs
					for (unsigned int k = 0; k < sourceModule.phaseIds.Size(); ++k)
					{
						newModule.phaseIds.Add(sourceModule.phaseIds[k]);
					}

					// Copy dependencies
					for (unsigned int k = 0; k < sourceModule.dependencies.Size(); ++k)
					{
						newModule.dependencies.Add(sourceModule.dependencies[k]);
					}

					targetModules.Add(newModule);
				}
			}
		}

		void ManifestComposer::MergeTransitions(
			const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseTransition, 32>& sourceTransitions,
			Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseTransition, 32>& targetTransitions)
		{
			// AC12 Rule 4: Union of all transitions (duplicates ignored)
			for (unsigned int i = 0; i < sourceTransitions.Size(); ++i)
			{
				const ApplicationManifest::PhaseTransition& sourceTrans = sourceTransitions[i];

				// Check if transition already exists
				bool found = false;
				for (unsigned int j = 0; j < targetTransitions.Size(); ++j)
				{
					const ApplicationManifest::PhaseTransition& targetTrans = targetTransitions[j];

					if (targetTrans.fromPhase == sourceTrans.fromPhase &&
						targetTrans.toPhase == sourceTrans.toPhase)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					targetTransitions.Add(sourceTrans);
				}
			}
		}

		void ManifestComposer::DeepMergeConfig(const Json::Value& source, Json::Value& target)
		{
			// Deep merge JSON objects recursively
			if (!source.isObject())
			{
				// For non-objects, source overwrites target
				target = source;
				return;
			}

			if (!target.isObject())
			{
				// If target is not an object, replace it
				target = source;
				return;
			}

			// Merge object members
			Json::Value::Members members = source.getMemberNames();
			for (Json::Value::Members::const_iterator it = members.begin(); it != members.end(); ++it)
			{
				const std::string& key = *it;

				if (target.isMember(key) && target[key].isObject() && source[key].isObject())
				{
					// Recursively merge nested objects
					DeepMergeConfig(source[key], target[key]);
				}
				else
				{
					// Overwrite or add new key
					target[key] = source[key];
				}
			}
		}

		ManifestValidationResult ManifestComposer::LoadManifestFromFile(const char* filePath, ApplicationManifest& outManifest)
		{
			// TODO: This is a stub. Will be implemented when ManifestLoader is complete.
			// For now, return an error indicating the file couldn't be loaded.
			// This will be replaced with actual JSON parsing logic.

			Dia::Core::Containers::String256 msg;
			msg.Format("ManifestComposer::LoadManifestFromFile not yet implemented (file: %s)", filePath);
			AddError(ManifestValidationResult::kImportNotFound, msg.AsCStr(), "composer");

			Dia::Core::Log::OutputVaradicLine("Warning: %s", msg.AsCStr());
			Dia::Core::Log::OutputVaradicLine("Note: This will be implemented when ApplicationManifestLoader is complete.");

			return ManifestValidationResult::kImportNotFound;
		}

		const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& ManifestComposer::GetErrors() const
		{
			return mErrors;
		}

		void ManifestComposer::ClearErrors()
		{
			mErrors.RemoveAll();
		}

		void ManifestComposer::AddError(ManifestValidationResult code, const char* message, const char* context)
		{
			ManifestValidationError error(code, message, context);
			mErrors.Add(error);
		}
	}
}
