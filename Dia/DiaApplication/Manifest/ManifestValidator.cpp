#include "ManifestValidator.h"

#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaCore/Core/Log.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Strings/String256.h>

namespace Dia
{
	namespace Application
	{
		//-----------------------------------------------------------------------------
		// ManifestValidationError
		//-----------------------------------------------------------------------------

		ManifestValidationError::ManifestValidationError()
			: code(ManifestValidationResult::kSuccess)
			, message("")
			, context("")
		{
		}

		ManifestValidationError::ManifestValidationError(ManifestValidationResult code, const char* message, const char* context)
			: code(code)
			, message(message)
			, context(context)
		{
		}

		const char* ManifestValidationError::ToString() const
		{
			static Dia::Core::Containers::String256 buffer;
			buffer.Clear();
			buffer.Format("[%s] %s: %s", GetResultString(code), context, message);
			return buffer.AsChar();
		}

		const char* ManifestValidationError::GetResultString(ManifestValidationResult result)
		{
			switch (result)
			{
			case ManifestValidationResult::kSuccess: return "Success";
			case ManifestValidationResult::kSchemaVersionUnsupported: return "SchemaVersionUnsupported";
			case ManifestValidationResult::kMissingRequiredField: return "MissingRequiredField";
			case ManifestValidationResult::kInvalidJSON: return "InvalidJSON";
			case ManifestValidationResult::kUnknownType: return "UnknownType";
			case ManifestValidationResult::kDuplicateInstanceId: return "DuplicateInstanceId";
			case ManifestValidationResult::kCircularDependency: return "CircularDependency";
			case ManifestValidationResult::kInvalidPhaseTransition: return "InvalidPhaseTransition";
			case ManifestValidationResult::kModuleMissingFromPhase: return "ModuleMissingFromPhase";
			case ManifestValidationResult::kImportNotFound: return "ImportNotFound";
			case ManifestValidationResult::kImportCycle: return "ImportCycle";
			default: return "Unknown";
			}
		}

		//-----------------------------------------------------------------------------
		// ManifestValidator
		//-----------------------------------------------------------------------------

		ManifestValidator::ManifestValidator()
			: mErrors()
		{
		}

		ManifestValidationResult ManifestValidator::Validate(const ApplicationManifest& manifest)
		{
			ClearErrors();

			// Validate schema version (AC1: schema version check)
			if (!ValidateSchema(manifest))
			{
				return ManifestValidationResult::kSchemaVersionUnsupported;
			}

			// Validate all types are registered (AC2: type registration check)
			ValidateTypes(manifest);

			// Validate each processing unit
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				const ApplicationManifest::ProcessingUnitEntry& entry = manifest.processingUnits[i];

				// Build context string for error messages
				Dia::Core::Containers::String256 context;
				context.Format("processing_units[%u]", i);

				// AC3: Unique instance IDs
				ValidateDuplicateInstanceIds(entry, context.AsChar());

				// AC4: Circular dependency detection
				ValidateDependencies(entry, context.AsChar());

				// AC5: Orphaned modules (warning only)
				ValidateOrphanedModules(entry, context.AsChar());

				// AC6: Phase transitions reference valid phases
				ValidatePhaseTransitions(entry, context.AsChar());

				// AC7: Module phase references are valid
				ValidateModulePhaseReferences(entry, context.AsChar());
			}

			// Return first error code if any, or success
			if (mErrors.Size() > 0)
			{
				return mErrors[0].code;
			}

			return ManifestValidationResult::kSuccess;
		}

		bool ManifestValidator::ValidateSchema(const ApplicationManifest& manifest)
		{
			// Only version 1 is currently supported
			if (manifest.version != 1)
			{
				Dia::Core::Containers::String256 msg;
				msg.Format("Unsupported schema version: %u (only version 1 is supported)", manifest.version);
				AddError(ManifestValidationResult::kSchemaVersionUnsupported, msg.AsChar(), "manifest");
				return false;
			}

			return true;
		}

		bool ManifestValidator::ValidateTypes(const ApplicationManifest& manifest)
		{
			bool allValid = true;
			ApplicationTypeRegistry& registry = ApplicationTypeRegistry::Instance();

			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				const ApplicationManifest::ProcessingUnitEntry& entry = manifest.processingUnits[i];

				// Validate ProcessingUnit type
				if (!registry.IsProcessingUnitTypeRegistered(entry.typeId))
				{
					Dia::Core::Containers::String256 context;
					context.Format("processing_units[%u].type_id", i);

					Dia::Core::Containers::String256 msg;
					msg.Format("ProcessingUnit type '%s' is not registered", entry.typeId.AsChar());
					AddError(ManifestValidationResult::kUnknownType, msg.AsChar(), context.AsChar());
					allValid = false;
				}

				// Validate Phase types
				for (unsigned int j = 0; j < entry.phases.Size(); ++j)
				{
					const ApplicationManifest::PhaseEntry& phase = entry.phases[j];
					if (!registry.IsPhaseTypeRegistered(phase.typeId))
					{
						Dia::Core::Containers::String256 context;
						context.Format("processing_units[%u].phases[%u].type_id", i, j);

						Dia::Core::Containers::String256 msg;
						msg.Format("Phase type '%s' is not registered", phase.typeId.AsChar());
						AddError(ManifestValidationResult::kUnknownType, msg.AsChar(), context.AsChar());
						allValid = false;
					}
				}

				// Validate Module types
				for (unsigned int j = 0; j < entry.modules.Size(); ++j)
				{
					const ApplicationManifest::ModuleEntry& module = entry.modules[j];
					if (!registry.IsModuleTypeRegistered(module.typeId))
					{
						Dia::Core::Containers::String256 context;
						context.Format("processing_units[%u].modules[%u].type_id", i, j);

						Dia::Core::Containers::String256 msg;
						msg.Format("Module type '%s' is not registered", module.typeId.AsChar());
						AddError(ManifestValidationResult::kUnknownType, msg.AsChar(), context.AsChar());
						allValid = false;
					}
				}
			}

			return allValid;
		}

		bool ManifestValidator::ValidateDuplicateInstanceIds(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context)
		{
			bool allValid = true;

			// Check for duplicate phase instance IDs
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, unsigned int, Dia::Core::StringCRCHashFunctor> phaseIds(32, 32);

			for (unsigned int i = 0; i < entry.phases.Size(); ++i)
			{
				const Dia::Core::StringCRC& instanceId = entry.phases[i].instanceId;

				if (phaseIds.ContainsKey(instanceId))
				{
					Dia::Core::Containers::String256 errorContext;
					errorContext.Format("%s.phases[%u]", context, i);

					Dia::Core::Containers::String256 msg;
					msg.Format("Duplicate phase instance ID: '%s'", instanceId.AsChar());
					AddError(ManifestValidationResult::kDuplicateInstanceId, msg.AsChar(), errorContext.AsChar());
					allValid = false;
				}
				else
				{
					phaseIds.Add(instanceId, i);
				}
			}

			// Check for duplicate module instance IDs
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, unsigned int, Dia::Core::StringCRCHashFunctor> moduleIds(64);

			for (unsigned int i = 0; i < entry.modules.Size(); ++i)
			{
				const Dia::Core::StringCRC& instanceId = entry.modules[i].instanceId;

				if (moduleIds.ContainsKey(instanceId))
				{
					Dia::Core::Containers::String256 errorContext;
					errorContext.Format("%s.modules[%u]", context, i);

					Dia::Core::Containers::String256 msg;
					msg.Format("Duplicate module instance ID: '%s'", instanceId.AsChar());
					AddError(ManifestValidationResult::kDuplicateInstanceId, msg.AsChar(), errorContext.AsChar());
					allValid = false;
				}
				else
				{
					moduleIds.Add(instanceId, i);
				}
			}

			return allValid;
		}

		bool ManifestValidator::ValidateDependencies(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context)
		{
			// Detect circular dependencies using DFS
			return DetectCircularDependencies(entry.modules, context);
		}

		bool ManifestValidator::DetectCircularDependencies(
			const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::ModuleEntry, 32>& modules,
			const char* context)
		{
			// Build adjacency list
			Dia::Core::Containers::HashTable<
				Dia::Core::StringCRC,
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>,
				Dia::Core::StringCRCHashFunctor> adjList(64);

			// Build module ID set for validation
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> moduleIdSet(64);

			for (unsigned int i = 0; i < modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& module = modules[i];
				moduleIdSet.Add(module.instanceId, true);

				// Initialize adjacency list for this module
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> deps;
				for (unsigned int j = 0; j < module.dependencies.Size(); ++j)
				{
					deps.Add(module.dependencies[j]);
				}
				adjList.Add(module.instanceId, deps);
			}

			// Validate all dependencies exist
			bool allDepsExist = true;
			for (unsigned int i = 0; i < modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& module = modules[i];
				for (unsigned int j = 0; j < module.dependencies.Size(); ++j)
				{
					const Dia::Core::StringCRC& depId = module.dependencies[j];
					if (!moduleIdSet.ContainsKey(depId))
					{
						Dia::Core::Containers::String256 errorContext;
						errorContext.Format("%s.modules[%u].dependencies[%u]", context, i, j);

						Dia::Core::Containers::String256 msg;
						msg.Format("Module '%s' depends on unknown module '%s'",
							module.instanceId.AsChar(), depId.AsChar());
						AddError(ManifestValidationResult::kUnknownType, msg.AsChar(), errorContext.AsChar());
						allDepsExist = false;
					}
				}
			}

			// DFS for cycle detection
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> visited(64);
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> recStack(64);
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> cycle;

			bool hasCycle = false;
			for (unsigned int i = 0; i < modules.Size(); ++i)
			{
				const Dia::Core::StringCRC& moduleId = modules[i].instanceId;

				bool* visitedPtr = visited.TryGetItem(moduleId);
				if (visitedPtr == nullptr || !(*visitedPtr))
				{
					cycle.Clear();
					if (HasCycleDFS(moduleId, adjList, visited, recStack, cycle))
					{
						// Build cycle path string
						Dia::Core::Containers::String256 cyclePath;
						for (unsigned int j = 0; j < cycle.Size(); ++j)
						{
							if (j > 0)
							{
								cyclePath.Append(" -> ");
							}
							cyclePath.Append(cycle[j].AsChar());
						}

						Dia::Core::Containers::String256 errorContext;
						errorContext.Format("%s.modules", context);

						Dia::Core::Containers::String256 msg;
						msg.Format("Circular dependency detected: %s", cyclePath.AsChar());
						AddError(ManifestValidationResult::kCircularDependency, msg.AsChar(), errorContext.AsChar());
						hasCycle = true;
					}
				}
			}

			return allDepsExist && !hasCycle;
		}

		bool ManifestValidator::HasCycleDFS(
			const Dia::Core::StringCRC& moduleId,
			const Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>, Dia::Core::StringCRCHashFunctor>& adjList,
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor>& visited,
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor>& recStack,
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& cycle)
		{
			visited.Add(moduleId, true);
			recStack.Add(moduleId, true);
			cycle.Add(moduleId);

			// Get dependencies
			const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>* depsPtr = adjList.TryGetItem(moduleId);
			if (depsPtr != nullptr)
			{
				const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& deps = *depsPtr;
				for (unsigned int i = 0; i < deps.Size(); ++i)
				{
					const Dia::Core::StringCRC& depId = deps[i];

					// If not visited, recurse
					bool* visitedPtr = visited.TryGetItem(depId);
					if (visitedPtr == nullptr || !(*visitedPtr))
					{
						if (HasCycleDFS(depId, adjList, visited, recStack, cycle))
						{
							return true;
						}
					}
					// If in recursion stack, we found a cycle
					else
					{
						bool* recPtr = recStack.TryGetItem(depId);
						if (recPtr != nullptr && *recPtr)
						{
							cycle.Add(depId); // Complete the cycle
							return true;
						}
					}
				}
			}

			// Remove from recursion stack before backtracking
			recStack.Add(moduleId, false);
			cycle.RemoveAt(cycle.Size() - 1);
			return false;
		}

		bool ManifestValidator::ValidatePhaseTransitions(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context)
		{
			bool allValid = true;

			// Build set of valid phase IDs
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> phaseIds(32, 32);
			for (unsigned int i = 0; i < entry.phases.Size(); ++i)
			{
				phaseIds.Add(entry.phases[i].instanceId, true);
			}

			// Validate initial phase exists
			if (entry.initialPhase.Value() != 0)
			{
				if (!phaseIds.ContainsKey(entry.initialPhase))
				{
					Dia::Core::Containers::String256 errorContext;
					errorContext.Format("%s.initial_phase", context);

					Dia::Core::Containers::String256 msg;
					msg.Format("Initial phase '%s' does not exist", entry.initialPhase.AsChar());
					AddError(ManifestValidationResult::kInvalidPhaseTransition, msg.AsChar(), errorContext.AsChar());
					allValid = false;
				}
			}

			// Validate all transitions reference valid phases
			for (unsigned int i = 0; i < entry.transitions.Size(); ++i)
			{
				const ApplicationManifest::PhaseTransition& transition = entry.transitions[i];

				if (!phaseIds.ContainsKey(transition.fromPhase))
				{
					Dia::Core::Containers::String256 errorContext;
					errorContext.Format("%s.transitions[%u].from", context, i);

					Dia::Core::Containers::String256 msg;
					msg.Format("Transition 'from' phase '%s' does not exist", transition.fromPhase.AsChar());
					AddError(ManifestValidationResult::kInvalidPhaseTransition, msg.AsChar(), errorContext.AsChar());
					allValid = false;
				}

				if (!phaseIds.ContainsKey(transition.toPhase))
				{
					Dia::Core::Containers::String256 errorContext;
					errorContext.Format("%s.transitions[%u].to", context, i);

					Dia::Core::Containers::String256 msg;
					msg.Format("Transition 'to' phase '%s' does not exist", transition.toPhase.AsChar());
					AddError(ManifestValidationResult::kInvalidPhaseTransition, msg.AsChar(), errorContext.AsChar());
					allValid = false;
				}
			}

			return allValid;
		}

		bool ManifestValidator::ValidateModulePhaseReferences(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context)
		{
			bool allValid = true;

			// Build set of valid phase IDs
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> phaseIds(32, 32);
			for (unsigned int i = 0; i < entry.phases.Size(); ++i)
			{
				phaseIds.Add(entry.phases[i].instanceId, true);
			}

			// Validate each module's phase references
			for (unsigned int i = 0; i < entry.modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& module = entry.modules[i];

				for (unsigned int j = 0; j < module.phaseIds.Size(); ++j)
				{
					const Dia::Core::StringCRC& phaseId = module.phaseIds[j];

					if (!phaseIds.ContainsKey(phaseId))
					{
						Dia::Core::Containers::String256 errorContext;
						errorContext.Format("%s.modules[%u].phases[%u]", context, i, j);

						Dia::Core::Containers::String256 msg;
						msg.Format("Module '%s' references non-existent phase '%s'",
							module.instanceId.AsChar(), phaseId.AsChar());
						AddError(ManifestValidationResult::kModuleMissingFromPhase, msg.AsChar(), errorContext.AsChar());
						allValid = false;
					}
				}
			}

			return allValid;
		}

		bool ManifestValidator::ValidateOrphanedModules(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context)
		{
			// AC5: Warn only, not an error
			for (unsigned int i = 0; i < entry.modules.Size(); ++i)
			{
				const ApplicationManifest::ModuleEntry& module = entry.modules[i];

				if (module.phaseIds.Size() == 0)
				{
					Dia::Core::Log::OutputVaradicLine("Warning: Module '%s' in %s has no phases (orphaned module)",
						module.instanceId.AsChar(), context);
				}
			}

			return true; // Always return true since this is a warning
		}

		const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& ManifestValidator::GetErrors() const
		{
			return mErrors;
		}

		void ManifestValidator::ClearErrors()
		{
			mErrors.RemoveAll();
		}

		void ManifestValidator::AddError(ManifestValidationResult code, const char* message, const char* context)
		{
			ManifestValidationError error(code, message, context);
			mErrors.Add(error);
		}
	}
}
