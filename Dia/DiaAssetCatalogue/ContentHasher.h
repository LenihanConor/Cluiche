#pragma once

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// ContentHasher
		//
		// Computes CRC32 content hashes for asset source files.
		// Used to detect when source content has changed since last catalogue build.
		//
		// ComputeHash: Reads entire file into memory, computes CRC32 of its bytes.
		// ComputeDirectoryHash: Enumerates all files in a directory recursively, sorts
		//   by relative path for determinism, computes per-file CRC32, then combines
		//   all per-file hashes into a single CRC32.
		//---------------------------------------------------------------------------------------------------------
		class ContentHasher
		{
		public:
			// CRC32 of a single source file's bytes.
			// Returns 0 if the file cannot be read.
			unsigned int ComputeHash(const char* filePath) const;

			// Combined CRC32 for a Folder asset (*.folder/ directory).
			// Enumerates all contained files recursively, sorts by relative path,
			// computes individual CRC32s, and combines them into a single hash.
			// Returns 0 if the directory cannot be enumerated or is empty.
			unsigned int ComputeDirectoryHash(const char* folderPath) const;
		};

	} // namespace AssetCatalogue
} // namespace Dia
