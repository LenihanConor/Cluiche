#include <DiaValueSchema/ValueSchemaService.h>

namespace Dia::ValueSchema
{
	using Dia::Core::StringCRC;

	ValueSchemaService::ValueSchemaService(const ValueSchemaRegistry& registry) : mRegistry(registry) {}

	const ValueInfo* ValueSchemaService::Find(StringCRC fullKey) const				{ return mRegistry.Find(fullKey); }
	const ValueInfo* ValueSchemaService::Find(StringCRC forFormat, StringCRC key) const	{ return mRegistry.Find(forFormat, key); }
	ValueInfoSpan ValueSchemaService::ForFormat(StringCRC formatId) const				{ return mRegistry.ForFormat(formatId); }
	const ValueInfo* ValueSchemaService::FindEvent(StringCRC eventKey) const			{ return mRegistry.FindEvent(eventKey); }
	unsigned int ValueSchemaService::Count() const										{ return mRegistry.Count(); }
}
