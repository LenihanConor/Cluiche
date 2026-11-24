#pragma once

// ValueSchemaService: read-only view over a published ValueSchemaRegistry.
// Published by ValueSchemaModule on the "ValueSchemaService" ServiceStream (VS-005: no singleton).
//
// Thin wrappers only. The registry is immutable once published, so every call is const and takes no
// lock; consumers on any PU may read concurrently. The service references (does not own) the
// registry: ValueSchemaModule owns both and keeps them alive until it is restarted or destroyed.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/RampRegistry.h>
#include <json/json.h>

namespace Dia::ValueSchema
{
	// Optional out-report for ExportSubset.
	struct SubsetReport
	{
		unsigned int mSkippedUnknown = 0;		// distinct requested keys that are not in the schema (one Warning each)
		unsigned int mReferencedUnresolved = 0;	// affects/related references that resolve to nothing (skipped silently)
	};

	class ValueSchemaService
	{
	public:
		explicit ValueSchemaService(const ValueSchemaRegistry& registry);
		// ramps may be null (no ramp registry): FindRamp then returns null. Not owned; must outlive the service.
		ValueSchemaService(const ValueSchemaRegistry& registry, const RampRegistry* ramps);
		ValueSchemaService(const ValueSchemaService&) = delete;
		ValueSchemaService& operator=(const ValueSchemaService&) = delete;

		const ValueInfo*	Find(Dia::Core::StringCRC fullKey) const;
		const ValueInfo*	Find(Dia::Core::StringCRC forFormat, Dia::Core::StringCRC key) const;
		ValueInfoSpan		ForFormat(Dia::Core::StringCRC formatId) const;	// entries in file order
		const ValueInfo*	FindEvent(Dia::Core::StringCRC eventKey) const;	// kind:event by key, any `for`
		unsigned int		Count() const;
		const ValueInfo*	At(unsigned int index) const;	// registration order; null when out of range
		unsigned int		Version() const;				// snapshot version, see ValueSchemaRegistry::Version()

		const RampData*		FindRamp(Dia::Core::StringCRC rampId) const;	// null for unknown ids
		unsigned int		RampCount() const;
	const RampData*		RampAt(unsigned int index) const;	// registration order; null when out of range or no ramps

		// Feature 6 AC 8. Writes { entries: [...], ramps: [...] } (same field names as the export file) for the given full keys,
		// the entries they reference through affects/related (ONE level, no transitive following), and every ramp named by
		// ui.ramp / ui.delta_ramp of those entries (ramp library order). Entries: requested order then referenced in first-seen
		// order, duplicates once. A reference is tried as "<entry for>/<ref>" first, then as written. An unknown requested key
		// is skipped with one Warning (VSC) per distinct key. Returns false only for keys == null with keyCount > 0.
		bool				ExportSubset(const Dia::Core::StringCRC* keys, unsigned int keyCount, Json::Value& out, SubsetReport* report = nullptr) const;

	private:
		const ValueSchemaRegistry&	mRegistry;
		const RampRegistry*			mRamps = nullptr;
	};
}
