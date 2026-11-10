#pragma once

// ValueSchemaLoader: parses one dia.value_schema v1 file (text) into ValueInfo entries.
// Reading from disk is the module's job; the loader takes text only.
//
// Enum colour policy: when an enum value gives `colour`, it is kept and mHasColour = true. When absent,
// mColour is set to the built-in categorical list colour for the value's index (index % 12) and
// mHasColour stays false, so consumers can tell a given colour from an assigned one.
//
// Strings never truncate silently: any field longer than its capacity fails the file, naming the entry.

#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaValueSchema/ValueInfo.h>

namespace Dia::ValueSchema
{
	struct LoadFailure
	{
		Dia::Core::Containers::String256	mFile;
		Dia::Core::Containers::String128	mEntry;
		Dia::Core::Containers::String512	mReason;
	};

	struct LoadResult
	{
		bool								mOk = false;
		LoadFailure							mFailure;	// valid when !mOk
		Dia::Core::Containers::String64		mFor;
		ValueList<ValueInfo>				mEntries;	// file order; empty when !mOk

		// Non-fatal diagnostics (also logged as Warning on channel VSC). One per unknown key in the
		// open `ui` / `history` blocks, once per (file, block, key). Observable without parsing logs.
		ValueList<Dia::Core::Containers::String512>	mWarnings;
	};

	class ValueSchemaLoader
	{
	public:
		// text = file contents; fileName is used for diagnostics and ValueInfo::mSourceFile.
		static LoadResult LoadFromText(const char* text, const char* fileName);
	};

	// Splits a record path: "species[].growth_rate" -> "species", "[]", "growth_rate".
	// Segments are separated by '.'; a "[]" suffix on a segment yields a separate "[]" segment.
	// Returns false if a segment exceeds the String64 capacity (outSegments then holds what was split).
	bool SplitRecordPath(const char* key, ValueList<Dia::Core::Containers::String64>& outSegments);
}
