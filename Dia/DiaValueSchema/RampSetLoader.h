#pragma once

// RampSetLoader: loads the Dia ramp set and every game ramp file into a RampRegistry. Plain class (no
// Application), so it is unit-testable; ValueSchemaModule resolves its aliases and drives it.
//
//  - Dia roots (AddDiaRoot): in each directory the file named exactly "ramps.json" (if present) is the Dia set.
//    Dia files load first so Dia ids always win.
//  - Game roots (AddGameRoot): every "*.ramps.json" found recursively, sorted full path (forward slashes),
//    de-duplicated across overlapping roots (same discovery as SchemaDirectoryLoader). Missing root = Warning.
//  - A whole-file failure (unreadable or rejected by RampLoader) is an Error, counted in mFilesFailed; the rest
//    load. A single bad ramp inside a good file is skipped (RampLoader logs it) and counted in mRampsRejected;
//    it is NOT a failed file. Registry rejections (clash / duplicate) also count in mRampsRejected.
//  - Does not publish the registry; the caller does.

#include <DiaValueSchema/RampRegistry.h>

namespace Dia::ValueSchema
{
	struct RampLoadStats
	{
		unsigned int mFilesFound = 0;		// ramp files discovered (Dia ramps.json + game *.ramps.json)
		unsigned int mFilesLoaded = 0;
		unsigned int mFilesFailed = 0;		// unreadable or rejected whole files
		unsigned int mRampsLoaded = 0;		// ramps registered
		unsigned int mRampsRejected = 0;	// skipped inside a file, or rejected by the registry
	};

	class RampSetLoader
	{
	public:
		RampSetLoader();
		~RampSetLoader();
		RampSetLoader(const RampSetLoader&) = delete;
		RampSetLoader& operator=(const RampSetLoader&) = delete;

		void			AddDiaRoot(const char* directory);
		void			AddGameRoot(const char* directory);

		RampLoadStats	Load(RampRegistry& registry);

	private:
		struct Impl;
		Impl* mImpl;
	};
}
