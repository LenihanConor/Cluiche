#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "DiaEditor/Plugin/EditorPluginRegistry.h"

#include <string>
#include <vector>

namespace Dia { namespace Game { struct DiaGameManifest; } }

namespace Dia
{
	namespace Editor
	{
		// Game-scope discovery: learn which plugin types are game-scoped without loading each game.
		// One collection path (CollectGameScope) is shared with EditorModel::LoadDiagameProject so the
		// game id and type list are identical in both routes. Everything here is tolerant: missing
		// directories and malformed files are logged and skipped, never fatal.
		namespace GameScopeDiscovery
		{
			typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, EditorPluginRegistry::kMaxPlugins> TypeList;

			// Reads the .diagame's editor.manifests (relative to the .diagame's directory), appends registered,
			// de-duplicated plugin types to outTypes, and returns the game id (StringCRC of NormalizeGamePath).
			Dia::Core::StringCRC CollectGameScope(const char* diagamePath, const Dia::Game::DiaGameManifest& manifest, TypeList& outTypes);

			// Loads the .diagame and tags every type it scopes via TagGameScoped. Does not activate a scope.
			// Returns false when the file is missing or malformed.
			bool TagGameFromDiagame(const char* diagamePath);

			// Recursively finds *.diagame under root (depth 0 = root's own files). Missing root -> empty.
			void ScanForDiagames(const char* root, unsigned int maxDepth, std::vector<std::string>& outPaths);

			// Tags the union of explicit game paths and every .diagame found under the roots.
			// Duplicates (by normalised path) are processed once. Returns the number of games tagged.
			unsigned int DiscoverAndTag(const std::vector<std::string>& roots, unsigned int maxDepth,
			                            const std::vector<std::string>& gamePaths);
		}
	}
}
