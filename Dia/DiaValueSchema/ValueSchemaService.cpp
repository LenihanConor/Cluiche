#include <DiaValueSchema/ValueSchemaService.h>

namespace Dia::ValueSchema
{
	using Dia::Core::StringCRC;

	ValueSchemaService::ValueSchemaService(const ValueSchemaRegistry& registry) : mRegistry(registry) {}
	ValueSchemaService::ValueSchemaService(const ValueSchemaRegistry& registry, const RampRegistry* ramps) : mRegistry(registry), mRamps(ramps) {}

	const ValueInfo* ValueSchemaService::Find(StringCRC fullKey) const				{ return mRegistry.Find(fullKey); }
	const ValueInfo* ValueSchemaService::Find(StringCRC forFormat, StringCRC key) const	{ return mRegistry.Find(forFormat, key); }
	ValueInfoSpan ValueSchemaService::ForFormat(StringCRC formatId) const				{ return mRegistry.ForFormat(formatId); }
	const ValueInfo* ValueSchemaService::FindEvent(StringCRC eventKey) const			{ return mRegistry.FindEvent(eventKey); }
	unsigned int ValueSchemaService::Count() const										{ return mRegistry.Count(); }
	const RampData* ValueSchemaService::FindRamp(StringCRC rampId) const				{ return mRamps ? mRamps->Find(rampId) : nullptr; }
	unsigned int ValueSchemaService::RampCount() const									{ return mRamps ? mRamps->Count() : 0u; }
}
