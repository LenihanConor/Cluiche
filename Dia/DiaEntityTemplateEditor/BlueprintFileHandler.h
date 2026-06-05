#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace EntityTemplateEditor
	{
		// Handles load/save of .diaentitytemplate, .diacamera, .dialight template files.
		// File format is owned by diaentitytemplate / DiaCamera2D / DiaLighting2D; this class
		// treats them as opaque JSON blobs for round-trip editing.
		class BlueprintFileHandler
		{
		public:
			BlueprintFileHandler();

			// Load a blueprint file. Returns true on success; fills errorOut on failure.
			bool Load(const char* path, Json::Value& outRoot,
			          char* errorOut, unsigned int errorCapacity);

			// Save a blueprint root back to its file. Returns true on success.
			bool Save(const char* path, const Json::Value& root,
			          char* errorOut, unsigned int errorCapacity);

			// Returns the top-level key for a given file extension
			// (e.g. ".diaentitytemplate" -> "entity_template").
			static const char* TopLevelKeyForExtension(const char* ext);

		private:
			static const unsigned int kCurrentPathLength = 512;
			char mCurrentPath[kCurrentPathLength];
		};
	}
}
