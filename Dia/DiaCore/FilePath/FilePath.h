#pragma once

#include "DiaCore/Strings/String512.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String8.h"

#include "DiaCore/FilePath/Path.h"

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// FilePath
		//
		// Abstract file path representation using path aliases for portability.
		//
		// Stores file paths as three components:
		//   - PathAlias: A registered root path (e.g., "data", "config", "exe")
		//   - PathAmendment: Optional subdirectory appended to the alias
		//   - FileName: The actual file name including extension
		//
		// USAGE:
		//   FilePath path("data", "textures", "player.png");
		//   FilePath::ResoledFilePath resolved;
		//   path.Resolve(resolved);  // -> "C:/Game/data/textures/player.png"
		//
		// BENEFITS:
		//   - Platform-independent path representation
		//   - Deferred resolution (resolve at runtime when paths are known)
		//   - Easy path reconfiguration via PathStore
		//---------------------------------------------------------------------------------------------------------------------------------
		class FilePath
		{
		public:
			// Type aliases for path components
			typedef Containers::String32 PathAmendment;    // Subdirectory appended to alias
			typedef Containers::String32 FileName;         // File name with extension
			typedef Containers::String8 FileType;          // File extension only
			typedef Containers::String512 ResoledFilePath; // Full resolved path string

			// Default constructor - creates empty file path
			FilePath();

			// Construct with alias and filename (no subdirectory)
			FilePath(const Path::Alias& pathAlias, const FileName& filename);

			// Construct with alias, subdirectory, and filename
			FilePath(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);

			// Initialize with alias and filename (no subdirectory)
			void Create(const Path::Alias& pathAlias, const FileName& filename);

			// Initialize with alias, subdirectory, and filename
			void Create(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);

			// Resolve the full file path by looking up the alias in PathStore
			// @param bufferToResovleInto - Buffer to store the resolved path
			// @return Reference to the resolved path string
			const ResoledFilePath& Resolve(ResoledFilePath& bufferToResovleInto)const;

			// Extract file extension from the filename
			FilePath::FileType ResolveFileType()const;

			// Get filename without the file extension
			FilePath::FileName ResolveFileNameWithoutFileType()const;

			// Accessors
			const Path::Alias& GetPathAlias()const { return mPathAlias; }
			const FilePath::PathAmendment& GetPathAmendment()const { return mPathAmendment; }
			const FilePath::FileName& GetFileName()const { return mFileName; }

		private:
			Path::Alias mPathAlias;          // Registered path alias (resolved via PathStore)
			PathAmendment mPathAmendment;    // Optional subdirectory
			FileName mFileName;              // File name including extension
		}; 
	}
}