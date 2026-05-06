#include "ManifestComposer.h"
#include "JsonApplicationManifestSerializer.h"

#include <DiaLogger/DiaLog.h>
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
				Dia::Core::Containers::String512 resolvedPath;
				ResolveRelativePath(filePath, currentManifest->imports[i], resolvedPath);

				result = ResolveImportsRecursive(resolvedPath.AsCStr(), outManifest);
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
			auto result = serializer.LoadFromFile(filePath, outManifest);
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
	}
}
