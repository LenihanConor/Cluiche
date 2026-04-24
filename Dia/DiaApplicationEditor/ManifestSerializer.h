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
				// Serialize manifest to a JSON value tree.
				// Returns true on success.
				static bool Serialize(const ApplicationManifest& manifest, Json::Value& outJson);

			private:
				static void SerializeProcessingUnit(const ApplicationManifest::ProcessingUnitEntry& pu, Json::Value& outJson);
				static void SerializePhase(const ApplicationManifest::PhaseEntry& phase, Json::Value& outJson);
				static void SerializeModule(const ApplicationManifest::ModuleEntry& module, Json::Value& outJson);
			};
		}
	}
}
