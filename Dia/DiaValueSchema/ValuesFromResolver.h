#pragma once

// ValuesFromResolver: fills an enum ValueInfo's mEnumValues from a data file's record array (`valuesFrom`).
//
// The data file is found by its `format`: for every schema sidecar "X.schema.json" the sibling "X.json" is a candidate, and the
// first candidate whose top-level "format" equals ValuesFrom::mFormat wins (the sidecar names the format it is `for`, so its data
// file sits next to it). Values are filled in record order from the configured id / label / colour / description fields.
//
//  - id is required on every record (non-empty string). A configured label / colour / description must be a string on every
//    record. Anything missing, wrongly typed or over capacity is a failure naming the reason; the entry is left unchanged.
//  - colour omitted from the valuesFrom object (or absent): EnumValue::mHasColour false and the categorical default colour for
//    the index, exactly like a literal `values` item without "colour". label / description omitted stay empty.
//  - An empty record array resolves to zero values and is not an error.
//
// Plain functions, no Application: unit-testable. Called by SchemaDirectoryLoader::LoadInto before registering.

#include <DiaValueSchema/ValueInfo.h>
#include <string>
#include <vector>

namespace Dia::ValueSchema
{
	// candidateDataFiles: full paths of "X.json" siblings of the discovered sidecars. On failure returns false and fills reason.
	bool ResolveValuesFrom(ValueInfo& entry, const std::vector<std::string>& candidateDataFiles, std::string& reason);
}
