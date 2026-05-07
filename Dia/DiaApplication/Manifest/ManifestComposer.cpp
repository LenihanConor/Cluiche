#include "ManifestComposer.h"
#include "JsonApplicationManifestSerializer.h"
#include "DiaGameManifestLoader.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Strings/stringutils.h>
#include <DiaCore/Json/external/json/json.h>

namespace
{
	// Build an absolute path for an import relative to the importing manifest's directory.
	// e.g. base="C:/Game/main.diaapp", importPath="sub/sim.diaapp" -> "C:/Game/sub/sim.diaapp"
	// If importPath is already absolute (starts with drive letter or '/'), returns it unchanged.
	void ResolveRelativePath(const char* baseFilePath, const char* importPath,
		Dia::Core::Containers::String512& outResolved)
	{
		// Check if import is already absolute
		if (importPath[0] == '/' || importPath[0] == '\\' ||
			(importPath[0] != '\0' && importPath[1] == ':'))
		{
			outResolved = importPath;
			return;
		}

		// Find last separator in base path to get the directory
		const char* lastSlash = nullptr;
		for (const char* p = baseFilePath; *p; ++p)
			if (*p == '/' || *p == '\\')
				lastSlash = p;

		if (lastSlash == nullptr)
		{
			outResolved = importPath;
			return;
		}

		// Copy directory prefix
		unsigned int dirLen = static_cast<unsigned int>(lastSlash - baseFilePath) + 1;
		char dirBuf[512];
		if (dirLen >= sizeof(dirBuf))
			dirLen = sizeof(dirBuf) - 1;
		memcpy(dirBuf, baseFilePath, dirLen);
		dirBuf[dirLen] = '\0';

		outResolved.Format("%s%s", dirBuf, importPath);
	}
}

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
			, mFileBuffer(new char[kFileBufferSize])
		{
		}

		ManifestComposer::~ManifestComposer()
		{
			delete[] mFileBuffer;
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
				ManifestValidationResult result = ResolveImportsRecursive(filePaths[i], outComposedManifest);
				if (result != ManifestValidationResult::kSuccess)
					return result;
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
			// Enforce max import depth (AI Review Q5: cap at 16 levels)
			if (mImportStack.Size() >= 16)
			{
				AddError(ManifestValidationResult::kImportCycle, "max import depth exceeded", filePath);
				return ManifestValidationResult::kImportCycle;
			}

			// Check for import cycle.
			// Fixed-string error message: path-string building with String512 locals at recursion
			// depth 3 caused stack overflow, so we emit a minimal message and use filePath as context.
			if (DetectImportCycle(filePath))
			{
				AddError(ManifestValidationResult::kImportCycle, "Import cycle detected", filePath);
				return ManifestValidationResult::kImportCycle;
			}

			// Check if already processed (diamond-import dedup)
			Dia::Core::StringCRC filePathCRC(filePath);
			if (mProcessedFiles.ContainsKey(filePathCRC))
				return ManifestValidationResult::kSuccess;

			// Load manifest from file (heap-allocated to avoid large stack frames at depth)
			ApplicationManifest* currentManifest = new ApplicationManifest();
			ManifestValidationResult result = LoadManifestFromFile(filePath, *currentManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				delete currentManifest;
				return result;
			}

			// Mark as being processed (for cycle detection)
			mImportStack.Add(filePath);

			// Recursively resolve imports (depth-first), merging directly into outManifest.
			// On error: mImportStack is not popped, leaving a stale entry. This is safe because
			// ManifestComposer is discarded after any error return — callers construct a new one.
			for (unsigned int i = 0; i < currentManifest->imports.Size(); ++i)
			{
				const TypedImport& import = currentManifest->imports[i];
				Dia::Core::Containers::String512 resolvedPath;
				ResolveRelativePath(filePath, import.path.AsCStr(), resolvedPath);

				if (import.type == TypedImport::ImportType::kStage)
				{
					result = ResolveStageImport(resolvedPath.AsCStr(), outManifest);
				}
				else
				{
					result = ResolveImportsRecursive(resolvedPath.AsCStr(), outManifest);
				}

				if (result != ManifestValidationResult::kSuccess)
				{
					delete currentManifest;
					return result;
				}
			}

			// Pop from stack
			mImportStack.RemoveAt(mImportStack.Size() - 1);

			// Mark as fully processed
			mProcessedFiles.Add(filePathCRC, true);

			// Merge current manifest into output; tag newly added entries with provenance
			unsigned int pusBefore = outManifest.processingUnits.Size();
			result = MergeManifests(*currentManifest, outManifest, filePath);
			if (result != ManifestValidationResult::kSuccess)
			{
				delete currentManifest;
				return result;
			}

			// Tag new PU entries with provenance (String256 has value semantics — safe with DynamicArrayC bitwise copy)
			for (unsigned int i = pusBefore; i < outManifest.processingUnits.Size(); ++i)
			{
				ApplicationManifest::ProcessingUnitEntry& pu = outManifest.processingUnits[i];
				pu.sourceManifestPath = filePath;
				for (unsigned int j = 0; j < pu.phases.Size(); ++j)
					pu.phases[j].sourceManifestPath = filePath;
				for (unsigned int j = 0; j < pu.modules.Size(); ++j)
					pu.modules[j].sourceManifestPath = filePath;
				for (unsigned int j = 0; j < pu.transitions.Size(); ++j)
					pu.transitions[j].sourceManifestPath = filePath;
			}

			delete currentManifest;
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

		ManifestValidationResult ManifestComposer::MergeManifests(const ApplicationManifest& source, ApplicationManifest& target, const char* sourceFilePath)
		{
			// Use source version if target is uninitialized
			if (target.version == 0)
			{
				target.version = source.version;
			}

			// AC5: Duplicate PU instance_id across distinct manifests → reject with kDuplicateInstanceId.
			// Each source PU must have a unique instance_id not already present in target.
			for (unsigned int i = 0; i < source.processingUnits.Size(); ++i)
			{
				const ApplicationManifest::ProcessingUnitEntry& sourcePU = source.processingUnits[i];

				for (unsigned int j = 0; j < target.processingUnits.Size(); ++j)
				{
					const ApplicationManifest::ProcessingUnitEntry& targetPU = target.processingUnits[j];
					if (targetPU.instanceId == sourcePU.instanceId)
					{
						Dia::Core::Containers::String256 msg;
						msg.Format("Duplicate processing_unit instance_id '%s' found in '%s' (already defined in '%s')",
							sourcePU.instanceId.AsChar(), sourceFilePath,
							targetPU.sourceManifestPath.IsEmpty() ? "root" : targetPU.sourceManifestPath.AsCStr());
						AddError(ManifestValidationResult::kDuplicateInstanceId, msg.AsCStr(), sourceFilePath);
						return ManifestValidationResult::kDuplicateInstanceId;
					}
				}

				// No duplicate found — add PU to target.
				// AddDefault + in-place reference avoids DynamicArrayC::operator= MemoryCopy,
				// which would alias Json::Value* config pointers → double-free on temp dtor.
				target.processingUnits.AddDefault();
				ApplicationManifest::ProcessingUnitEntry& newPU =
					target.processingUnits[target.processingUnits.Size() - 1];

				newPU.typeId = sourcePU.typeId;
				newPU.instanceId = sourcePU.instanceId;
				newPU.frequencyHz = sourcePU.frequencyHz;
				newPU.dedicatedThread = sourcePU.dedicatedThread;
				newPU.root = sourcePU.root;
				newPU.initialPhase = sourcePU.initialPhase;

				if (sourcePU.config != nullptr)
				{
					newPU.config = new Json::Value(*sourcePU.config);
				}

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

					for (unsigned int m = 0; m < sourceModule.phaseIds.Size(); ++m)
					{
						newModule.phaseIds.Add(sourceModule.phaseIds[m]);
					}

					for (unsigned int m = 0; m < sourceModule.dependencies.Size(); ++m)
					{
						newModule.dependencies.Add(sourceModule.dependencies[m]);
					}

					newPU.modules.Add(newModule);
				}

				for (unsigned int k = 0; k < sourcePU.transitions.Size(); ++k)
				{
					newPU.transitions.Add(sourcePU.transitions[k]);
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

			return ManifestValidationResult::kSuccess;
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
			JsonApplicationManifestSerializer serializer;
			auto result = serializer.LoadFromFile(filePath, outManifest, mFileBuffer, kFileBufferSize);
			if (!result)
			{
				const char* err = result.error ? result.error : "unknown error";
				ManifestValidationResult code;
				if (strcmp(err, "file read error") == 0)
					code = ManifestValidationResult::kImportNotFound;
				else if (strcmp(err, "json parse error") == 0)
					code = ManifestValidationResult::kInvalidJSON;
				else
					code = ManifestValidationResult::kMissingRequiredField;

				Dia::Core::Containers::String256 msg;
				msg.Format("Failed to load manifest: %s (%s)", filePath, err);
				AddError(code, msg.AsCStr(), filePath);
				return code;
			}

			return ManifestValidationResult::kSuccess;
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

		ManifestValidationResult ManifestComposer::ComposeFromGameFile(
			const char* diagamePath,
			ApplicationManifest& outComposedManifest,
			DiaGameManifest& outGameManifest)
		{
			DIA_ASSERT(diagamePath != nullptr, "Game file path cannot be null");

			ClearErrors();
			mImportStack.RemoveAll();
			mProcessedFiles.Clear();

			ManifestValidationResult result = DiaGameManifestLoader::LoadGameFile(diagamePath, outGameManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				AddError(result, "Failed to parse .diagame file", diagamePath);
				return result;
			}

			for (unsigned int i = 0; i < outGameManifest.imports.Size(); ++i)
			{
				const TypedImport& import = outGameManifest.imports[i];
				Dia::Core::Containers::String512 resolvedPath;
				ResolveRelativePath(diagamePath, import.path.AsCStr(), resolvedPath);

				if (import.type == TypedImport::ImportType::kStage)
				{
					result = ResolveStageImport(resolvedPath.AsCStr(), outComposedManifest);
				}
				else
				{
					result = ResolveImportsRecursive(resolvedPath.AsCStr(), outComposedManifest);
				}

				if (result != ManifestValidationResult::kSuccess)
					return result;
			}

			return ManifestValidationResult::kSuccess;
		}

		ManifestValidationResult ManifestComposer::ResolveStageImport(
			const char* diastagePath,
			ApplicationManifest& outManifest)
		{
			DiaStageManifest stageManifest;
			ManifestValidationResult result = DiaGameManifestLoader::LoadStageFile(diastagePath, stageManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				AddError(result, "Failed to parse .diastage file", diastagePath);
				return result;
			}

			// Resolve the stage's .diaapp relative to the .diastage file
			Dia::Core::Containers::String512 resolvedManifestPath;
			ResolveRelativePath(diastagePath, stageManifest.manifestPath.AsCStr(), resolvedManifestPath);

			// Load the stage .diaapp
			ApplicationManifest* stageAppManifest = new ApplicationManifest();
			result = LoadManifestFromFile(resolvedManifestPath.AsCStr(), *stageAppManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				delete stageAppManifest;
				return result;
			}

			// Check metadata.type == "stage"
			bool isStageManifest = false;
			if (stageAppManifest->metadata != nullptr &&
				stageAppManifest->metadata->isMember("type") &&
				(*stageAppManifest->metadata)["type"].asString() == "stage")
			{
				isStageManifest = true;
			}

			if (isStageManifest)
			{
				result = MergeStageManifest(*stageAppManifest, outManifest, resolvedManifestPath.AsCStr());
			}
			else
			{
				DIA_LOG_WARNING("Application",
					".diastage '%s' points to a .diaapp without metadata.type=\"stage\" — treating as regular manifest",
					diastagePath);
				result = MergeManifests(*stageAppManifest, outManifest, resolvedManifestPath.AsCStr());
			}

			delete stageAppManifest;
			return result;
		}

		ManifestValidationResult ManifestComposer::MergeStageManifest(
			const ApplicationManifest& stageManifest,
			ApplicationManifest& target,
			const char* sourceFilePath)
		{
			// Stage manifests use metadata to hold stage_phases, stage_transitions, stage_modules
			// These are stored in the manifest's metadata JSON (parsed by JsonApplicationManifestSerializer)
			if (stageManifest.metadata == nullptr)
			{
				AddError(ManifestValidationResult::kMissingRequiredField, "Stage manifest missing metadata", sourceFilePath);
				return ManifestValidationResult::kMissingRequiredField;
			}

			const Json::Value& meta = *stageManifest.metadata;

			// Validate requires_phases if present
			if (meta.isMember("requires_phases") && meta["requires_phases"].isArray())
			{
				const Json::Value& reqPhases = meta["requires_phases"];
				for (unsigned int r = 0; r < reqPhases.size(); ++r)
				{
					if (!reqPhases[r].isString()) continue;
					Dia::Core::StringCRC reqId(reqPhases[r].asCString());
					bool found = false;
					for (unsigned int p = 0; p < target.processingUnits.Size() && !found; ++p)
					{
						for (unsigned int q = 0; q < target.processingUnits[p].phases.Size(); ++q)
						{
							if (target.processingUnits[p].phases[q].instanceId == reqId)
							{
								found = true;
								break;
							}
						}
					}
					if (!found)
					{
						Dia::Core::Containers::String256 msg;
						msg.Format("Stage requires phase '%s' but it was not found in any PU", reqPhases[r].asCString());
						AddError(ManifestValidationResult::kMissingRequiredField, msg.AsCStr(), sourceFilePath);
						return ManifestValidationResult::kMissingRequiredField;
					}
				}
			}

			// Process stage_phases — inject into target PUs
			if (meta.isMember("stage_phases") && meta["stage_phases"].isArray())
			{
				const Json::Value& stagePhases = meta["stage_phases"];
				for (unsigned int i = 0; i < stagePhases.size(); ++i)
				{
					const Json::Value& sp = stagePhases[i];
					if (!sp.isMember("type_id") || !sp.isMember("instance_id") || !sp.isMember("target_processing_unit"))
						continue;

					Dia::Core::StringCRC targetPuId(sp["target_processing_unit"].asCString());
					Dia::Core::StringCRC phaseInstanceId(sp["instance_id"].asCString());

					// Find target PU
					int puIdx = -1;
					for (unsigned int p = 0; p < target.processingUnits.Size(); ++p)
					{
						if (target.processingUnits[p].instanceId == targetPuId)
						{
							puIdx = static_cast<int>(p);
							break;
						}
					}

					if (puIdx < 0)
					{
						Dia::Core::Containers::String256 msg;
						msg.Format("Stage phase targets PU '%s' which does not exist", sp["target_processing_unit"].asCString());
						AddError(ManifestValidationResult::kMissingRequiredField, msg.AsCStr(), sourceFilePath);
						return ManifestValidationResult::kMissingRequiredField;
					}

					// Check for duplicate phase ID
					auto& targetPU = target.processingUnits[static_cast<unsigned int>(puIdx)];
					for (unsigned int q = 0; q < targetPU.phases.Size(); ++q)
					{
						if (targetPU.phases[q].instanceId == phaseInstanceId)
						{
							Dia::Core::Containers::String256 msg;
							msg.Format("Duplicate phase instance_id '%s' in PU '%s'", sp["instance_id"].asCString(), sp["target_processing_unit"].asCString());
							AddError(ManifestValidationResult::kDuplicateInstanceId, msg.AsCStr(), sourceFilePath);
							return ManifestValidationResult::kDuplicateInstanceId;
						}
					}

					// Add phase to target PU
					ApplicationManifest::PhaseEntry newPhase;
					newPhase.typeId = Dia::Core::StringCRC(sp["type_id"].asCString());
					newPhase.instanceId = phaseInstanceId;
					newPhase.sourceManifestPath = sourceFilePath;
					if (sp.isMember("config"))
						newPhase.config = new Json::Value(sp["config"]);
					targetPU.phases.Add(newPhase);
				}
			}

			// Process stage_transitions
			if (meta.isMember("stage_transitions") && meta["stage_transitions"].isArray())
			{
				const Json::Value& stageTransitions = meta["stage_transitions"];
				for (unsigned int i = 0; i < stageTransitions.size(); ++i)
				{
					const Json::Value& st = stageTransitions[i];
					if (!st.isMember("from") || !st.isMember("to") || !st.isMember("target_processing_unit"))
						continue;

					Dia::Core::StringCRC targetPuId(st["target_processing_unit"].asCString());
					int puIdx = -1;
					for (unsigned int p = 0; p < target.processingUnits.Size(); ++p)
					{
						if (target.processingUnits[p].instanceId == targetPuId)
						{
							puIdx = static_cast<int>(p);
							break;
						}
					}
					if (puIdx < 0) continue;

					ApplicationManifest::PhaseTransition t;
					t.fromPhase = Dia::Core::StringCRC(st["from"].asCString());
					t.toPhase = Dia::Core::StringCRC(st["to"].asCString());
					t.sourceManifestPath = sourceFilePath;
					target.processingUnits[static_cast<unsigned int>(puIdx)].transitions.Add(t);
				}
			}

			// Process stage_modules
			if (meta.isMember("stage_modules") && meta["stage_modules"].isArray())
			{
				const Json::Value& stageModules = meta["stage_modules"];
				for (unsigned int i = 0; i < stageModules.size(); ++i)
				{
					const Json::Value& sm = stageModules[i];
					if (!sm.isMember("type_id") || !sm.isMember("instance_id") || !sm.isMember("target_processing_unit"))
						continue;

					Dia::Core::StringCRC targetPuId(sm["target_processing_unit"].asCString());
					int puIdx = -1;
					for (unsigned int p = 0; p < target.processingUnits.Size(); ++p)
					{
						if (target.processingUnits[p].instanceId == targetPuId)
						{
							puIdx = static_cast<int>(p);
							break;
						}
					}
					if (puIdx < 0) continue;

					auto& targetPU = target.processingUnits[static_cast<unsigned int>(puIdx)];
					Dia::Core::StringCRC moduleInstanceId(sm["instance_id"].asCString());

					// Check if module already exists in target PU — if so, extend its phaseIds
					int existingModIdx = -1;
					for (unsigned int m = 0; m < targetPU.modules.Size(); ++m)
					{
						if (targetPU.modules[m].instanceId == moduleInstanceId)
						{
							existingModIdx = static_cast<int>(m);
							break;
						}
					}

					if (existingModIdx >= 0)
					{
						// Merge: append stage phase_ids to existing module
						auto& existingMod = targetPU.modules[static_cast<unsigned int>(existingModIdx)];
						if (sm.isMember("phase_ids") && sm["phase_ids"].isArray())
						{
							const Json::Value& ids = sm["phase_ids"];
							for (unsigned int j = 0; j < ids.size(); ++j)
							{
								if (!ids[j].isString()) continue;
								Dia::Core::StringCRC phaseId(ids[j].asCString());
								// Avoid duplicate phase_ids
								bool alreadyPresent = false;
								for (unsigned int k = 0; k < existingMod.phaseIds.Size(); ++k)
								{
									if (existingMod.phaseIds[k] == phaseId)
									{
										alreadyPresent = true;
										break;
									}
								}
								if (!alreadyPresent)
									existingMod.phaseIds.Add(phaseId);
							}
						}
					}
					else
					{
						// New module — add it to the PU
						ApplicationManifest::ModuleEntry newModule;
						newModule.typeId = Dia::Core::StringCRC(sm["type_id"].asCString());
						newModule.instanceId = moduleInstanceId;
						newModule.sourceManifestPath = sourceFilePath;

						if (sm.isMember("phase_ids") && sm["phase_ids"].isArray())
						{
							const Json::Value& ids = sm["phase_ids"];
							for (unsigned int j = 0; j < ids.size(); ++j)
								if (ids[j].isString())
									newModule.phaseIds.Add(Dia::Core::StringCRC(ids[j].asCString()));
						}

						if (sm.isMember("config"))
							newModule.config = new Json::Value(sm["config"]);

						targetPU.modules.Add(newModule);
					}
				}
			}

			return ManifestValidationResult::kSuccess;
		}
	}
}
