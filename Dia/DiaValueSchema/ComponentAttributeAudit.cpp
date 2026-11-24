#include <DiaValueSchema/ComponentAttributeAudit.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::ValueSchema
{
	namespace
	{
		constexpr unsigned int kKindEmptyDescription = 0;
		constexpr unsigned int kKindUnknownField = 1;
	}

	bool ComponentAttributeAudit::MarkSeen(unsigned int kind, unsigned int typeCrc, unsigned int fieldCrc, unsigned int& overflow)
	{
		for (unsigned int i = 0; i < mSeenCount; ++i)
		{
			if (mSeen[i].mKind == kind && mSeen[i].mTypeCrc == typeCrc && mSeen[i].mFieldCrc == fieldCrc) { return false; }
		}
		if (mSeenCount >= kMaxSeen) { ++overflow; return true; }
		mSeen[mSeenCount++] = Seen{ kind, typeCrc, fieldCrc };
		return true;
	}

	ComponentAttributeAuditReport ComponentAttributeAudit::Run(const Dia::Reflect::FieldAttributeRegistry& attrs,
		const Dia::Entity::ComponentTypeDesc* const* descs, unsigned int count)
	{
		ComponentAttributeAuditReport rep;
		rep.mEntriesUsed = attrs.EntryCount();
		rep.mDroppedCount = attrs.DroppedCount();
		if (descs == nullptr) { return rep; }

		// Empty descriptions: walk the component fields.
		for (unsigned int c = 0; c < count; ++c)
		{
			const Dia::Entity::ComponentTypeDesc* desc = descs[c];
			if (desc == nullptr || desc->debugName == nullptr) { continue; }
			const unsigned int typeCrc = Dia::Core::StringCRC(desc->debugName).Value();
			for (unsigned int f = 0; f < desc->fieldCount; ++f)
			{
				const Dia::Entity::FieldDesc& field = desc->fields[f];
				const Dia::Reflect::DescribeAttribute* d =
					attrs.FindAttribute<Dia::Reflect::DescribeAttribute>(typeCrc, field.nameCrc);
				if (d == nullptr || (d->description != nullptr && d->description[0] != '\0')) { continue; }
				++rep.mEmptyDescriptions;
				if (MarkSeen(kKindEmptyDescription, typeCrc, field.nameCrc, rep.mSeenOverflow))
				{
					++rep.mNewWarnings;
					DIA_LOG_WARNING("VSC", "Component field '%s.%s' has a describe with an empty description", desc->debugName, field.name);
				}
			}
		}

		// Describes for names that are not fields of the class: walk the attribute entries.
		for (unsigned int e = 0; e < attrs.EntryCount(); ++e)
		{
			const Dia::Reflect::FieldKey& key = attrs.EntryAt(e).key;
			// AC 13 covers describes only; other attributes on unlisted fields are not this audit's concern.
			if (attrs.FindAttribute<Dia::Reflect::DescribeAttribute>(key.typeCrc, key.fieldCrc) == nullptr) { continue; }
			for (unsigned int c = 0; c < count; ++c)
			{
				const Dia::Entity::ComponentTypeDesc* desc = descs[c];
				if (desc == nullptr || desc->debugName == nullptr) { continue; }
				if (Dia::Core::StringCRC(desc->debugName).Value() != key.typeCrc) { continue; }
				bool known = false;
				for (unsigned int f = 0; f < desc->fieldCount && !known; ++f) { known = desc->fields[f].nameCrc == key.fieldCrc; }
				if (known) { break; }
				++rep.mUnknownFieldDescribes;
				if (MarkSeen(kKindUnknownField, key.typeCrc, key.fieldCrc, rep.mSeenOverflow))
				{
					++rep.mNewWarnings;
					DIA_LOG_WARNING("VSC", "Describe registered for '%s' field crc 0x%08X, which is not a field of the class",
						desc->debugName, key.fieldCrc);
				}
				break;
			}
		}
		return rep;
	}

	ComponentAttributeAuditReport ComponentAttributeAudit::Run(const Dia::Reflect::FieldAttributeRegistry& attrs)
	{
		Dia::Entity::ComponentRegistry& components = Dia::Entity::ComponentRegistry::Get();
		const unsigned int count = components.GetCount();
		const Dia::Entity::ComponentTypeDesc** descs = new const Dia::Entity::ComponentTypeDesc*[count > 0 ? count : 1];
		for (unsigned int i = 0; i < count; ++i) { descs[i] = &components.GetByIndex(i); }
		ComponentAttributeAuditReport rep = Run(attrs, descs, count);
		delete[] descs;
		return rep;
	}
}
