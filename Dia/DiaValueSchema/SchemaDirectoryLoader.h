#pragma once

// SchemaDirectoryLoader: discovers *.schema.json files under a set of resolved directories and loads
// them into a ValueSchemaRegistry. Plain class (no Application), so it is unit-testable;
// ValueSchemaModule resolves its `schema_roots` aliases and drives it.
//
//  - Discovery is recursive. Only names ending in ".schema.json" are loaded.
//  - Load order is the sorted full file path (absolute, normalised to '/' separators) over the whole
//    combined list, so it is independent of directory iteration order and of root order.
//  - A file reachable from two roots (same root twice, or overlapping roots) is loaded once.
//  - A missing / non-directory root is a Warning (channel VSC), counted in mRootsMissing; not fatal.
//  - A bad file (unreadable or rejected by ValueSchemaLoader) is skipped with an Error naming file,
//    entry and reason; the rest load. Duplicate full keys: first loaded wins (the registry logs both).
//  - Does not publish the registry; the caller does.

#include <DiaValueSchema/ValueSchemaRegistry.h>

namespace Dia::ValueSchema
{
	struct SchemaLoadStats
	{
		unsigned int mFilesFound = 0;		// unique *.schema.json files discovered
		unsigned int mFilesLoaded = 0;		// files the loader accepted
		unsigned int mFilesFailed = 0;		// unreadable or rejected files
		unsigned int mEntriesLoaded = 0;	// entries registered (RegisterResult::Ok)
		unsigned int mEntriesRejected = 0;	// entries the registry rejected (duplicate / collision)
		unsigned int mRootsMissing = 0;		// roots that were not an existing directory (or unresolved aliases)
	};

	class SchemaDirectoryLoader
	{
	public:
		SchemaDirectoryLoader();
		~SchemaDirectoryLoader();
		SchemaDirectoryLoader(const SchemaDirectoryLoader&) = delete;
		SchemaDirectoryLoader& operator=(const SchemaDirectoryLoader&) = delete;

		void			AddRoot(const char* directory);
		unsigned int	RootCount() const;

		// Discovers and sorts the file list (replacing any earlier list). Returns the file count.
		unsigned int	Discover();
		unsigned int	FileCount() const;
		const char*		FileAt(unsigned int index) const;	// normalised full path, '/' separators

		// Discover() then load every file, in order, into registry. Does not Publish.
		SchemaLoadStats	LoadInto(ValueSchemaRegistry& registry);

	private:
		struct Impl;
		Impl* mImpl;
	};
}
