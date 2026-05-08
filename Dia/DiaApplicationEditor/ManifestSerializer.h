#pragma once

#include <DiaApplication/Manifest/ApplicationManifest.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Application
	{
		namespace Editor
		{
			// Serializes an ApplicationManifest IR back to JSON.
			// Inverse of the parsing side in ApplicationManifestLoader.
			class ManifestSerializer
			{
			public:
				// Serialize manifest to a JSON value tree (all PUs — for UI display).
				// Returns true on success.
				static bool Serialize(const ApplicationManifest& manifest, Json::Value& outJson);

				// Serialize only local PUs (sourceManifestPath empty or matching localFilePath).
				// Used when saving to disk to avoid flattening imports into the root file.
				static bool SerializeLocal(const ApplicationManifest& manifest, const char* localFilePath, Json::Value& outJson);

			private:
				static bool IsLocalEntry(const Dia::Core::Containers::String256& sourceManifestPath, const char* localFilePath);
				static void SerializeProcessingUnit(const ApplicationManifest::ProcessingUnitEntry& pu, Json::Value& outJson);
				static void SerializeProcessingUnitLocal(const ApplicationManifest::ProcessingUnitEntry& pu, const char* localFilePath, Json::Value& outJson);
				static void SerializePhase(const ApplicationManifest::PhaseEntry& phase, Json::Value& outJson);
				static void SerializeModule(const ApplicationManifest::ModuleEntry& module, Json::Value& outJson);
			};
		}
	}
}
