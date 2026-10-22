#pragma once

// RampLoader: parses one dia.ramps v1 file (text) into RampData. Reading from disk is the caller's job.
//
// Failure policy: a bad file shell (unparsable JSON, non-object root, wrong format/version, `ramps` missing or
// not an array, unknown top-level key) rejects the WHOLE file (mOk = false, mRamps empty). A single bad ramp
// is skipped with an Error naming its id (or index when the id is unusable); the other ramps still load.

#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/ValueInfo.h>

namespace Dia::ValueSchema
{
	struct RampLoadResult
	{
		bool									mOk = false;
		Dia::Core::Containers::String256		mFile;
		Dia::Core::Containers::String512		mFileReason;	// valid when !mOk
		ValueList<RampData>						mRamps;			// file order; empty when !mOk
		unsigned								mRampsRejected = 0;
		ValueList<Dia::Core::Containers::String512>	mRampErrors;	// one per skipped ramp
	};

	class RampLoader
	{
	public:
		// text = file contents; fileName is used for diagnostics.
		static RampLoadResult LoadFromText(const char* text, const char* fileName);
	};
}
