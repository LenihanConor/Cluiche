#pragma once

// ValueSchemaRegistry: entries keyed by full key "<for>/<key>" (StringCRC of the whole string).
//
// Hash design: Find(fullKey) uses the CRC of "<for>/<key>". Find(forFormat, key) cannot combine two
// already-hashed CRCs equivalently, so the registry keeps a per-format table keyed by the key's own
// CRC; both lookups resolve to the same stored ValueInfo. (StringCRC::AsChar truncates at 63 chars, so
// rebuilding the string from hashed inputs would not be safe.)
//
// Immutable after Publish(): reads take no lock. Returned pointers stay valid for the registry's life.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaValueSchema/ValueInfo.h>

namespace Dia::ValueSchema
{
	enum class RegisterResult
	{
		Ok, DuplicateKey, HashCollision, DuplicateEventKey, AlreadyPublished
	};

	struct ValueInfoSpan
	{
		const ValueInfo* const*	mItems = nullptr;
		unsigned int			mCount = 0;
	};

	class ValueSchemaRegistry
	{
	public:
		ValueSchemaRegistry();
		~ValueSchemaRegistry();
		ValueSchemaRegistry(const ValueSchemaRegistry&) = delete;
		ValueSchemaRegistry& operator=(const ValueSchemaRegistry&) = delete;

		RegisterResult	Register(const ValueInfo& info);
		// TEST ONLY: forces the full-key hash to fullKeyHashOverride to exercise collision handling.
		RegisterResult	Register(const ValueInfo& info, Dia::Core::StringCRC fullKeyHashOverride);

		void			Publish();
		bool			IsPublished() const;

		const ValueInfo*	Find(Dia::Core::StringCRC fullKey) const;
		const ValueInfo*	Find(Dia::Core::StringCRC forFormat, Dia::Core::StringCRC key) const;
		ValueInfoSpan		ForFormat(Dia::Core::StringCRC formatId) const;
		const ValueInfo*	FindEvent(Dia::Core::StringCRC eventKey) const;
		unsigned int		Count() const;

	private:
		struct Impl;
		RegisterResult	RegisterInternal(const ValueInfo& info, const Dia::Core::StringCRC* hashOverride);
		Impl*			mImpl;
	};
}
