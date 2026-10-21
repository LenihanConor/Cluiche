#pragma once

// ValueSchemaService: read-only view over a published ValueSchemaRegistry.
// Published by ValueSchemaModule on the "ValueSchemaService" ServiceStream (VS-005: no singleton).
//
// Thin wrappers only. The registry is immutable once published, so every call is const and takes no
// lock; consumers on any PU may read concurrently. The service references (does not own) the
// registry: ValueSchemaModule owns both and keeps them alive until it is restarted or destroyed.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>

namespace Dia::ValueSchema
{
	class ValueSchemaService
	{
	public:
		explicit ValueSchemaService(const ValueSchemaRegistry& registry);
		ValueSchemaService(const ValueSchemaService&) = delete;
		ValueSchemaService& operator=(const ValueSchemaService&) = delete;

		const ValueInfo*	Find(Dia::Core::StringCRC fullKey) const;
		const ValueInfo*	Find(Dia::Core::StringCRC forFormat, Dia::Core::StringCRC key) const;
		ValueInfoSpan		ForFormat(Dia::Core::StringCRC formatId) const;	// entries in file order
		const ValueInfo*	FindEvent(Dia::Core::StringCRC eventKey) const;	// kind:event by key, any `for`
		unsigned int		Count() const;

	private:
		const ValueSchemaRegistry& mRegistry;
	};
}
