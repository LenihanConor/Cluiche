#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		struct StageEntry
		{
			char name[256];
			char stagePath[512];   // absolute path to .diastage
			char scenePath[512];   // absolute path to .diascene, empty if not set
		};

		// Parses a .diagame file and builds a list of StageEntry records.
		// Scene path comes from an optional "scene" field in the .diastage JSON.
		// All paths are returned as absolute paths.
		class ProjectContextManager
		{
		public:
			// Returns JSON array of {name, stagePath, scenePath} objects.
			Json::Value BuildStageListJson(const char* diagamePath) const;

			// Given the absolute scene path, loads and returns the raw JSON root.
			// Returns null Value on failure; fills errBuf if provided.
			Json::Value LoadScene(const char* scenePath, char* errBuf, int errBufSize) const;
		};
	}
}
