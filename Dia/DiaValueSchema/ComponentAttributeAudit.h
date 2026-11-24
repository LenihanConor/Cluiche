#pragma once

// ComponentAttributeAudit: read-only check of the DiaCore FieldAttributeRegistry against the registered ECS
// components (Feature 7). One Run reports:
//   - mDroppedCount           registrations the attribute registry lost to a capacity limit (module: Error + Degraded)
//   - mEntriesUsed            distinct (type, field) keys in use
//   - mEmptyDescriptions      described component fields whose description is null or empty
//   - mUnknownFieldDescribes  attribute keys whose type is a given component but whose field is none of its fields
// Warnings are logged on channel VSC once per (kind, class, field) pair for the life of the audit object, so a
// repeated Run (module restart) does not re-warn. The seen list holds kMaxSeen pairs; pairs past that are still
// counted in the report and still warn each Run, and mSeenOverflow counts them.
// Counts are totals for the run; mNewWarnings is how many warnings this run actually logged.

#include <DiaCore/Reflect/FieldAttributes.h>

namespace Dia::Entity { struct ComponentTypeDesc; }

namespace Dia::ValueSchema
{
	struct ComponentAttributeAuditReport
	{
		unsigned int	mEntriesUsed = 0;
		unsigned int	mDroppedCount = 0;
		unsigned int	mEmptyDescriptions = 0;
		unsigned int	mUnknownFieldDescribes = 0;
		unsigned int	mNewWarnings = 0;
		unsigned int	mSeenOverflow = 0;
	};

	class ComponentAttributeAudit
	{
	public:
		static constexpr unsigned int kMaxSeen = 256;

		// Audits the given descriptors.
		ComponentAttributeAuditReport Run(const Dia::Reflect::FieldAttributeRegistry& attrs,
			const Dia::Entity::ComponentTypeDesc* const* descs, unsigned int count);
		// Audits every component in Dia::Entity::ComponentRegistry::Get().
		ComponentAttributeAuditReport Run(const Dia::Reflect::FieldAttributeRegistry& attrs);

		// Forget the warned pairs.
		void Reset() { mSeenCount = 0; }

	private:
		struct Seen
		{
			unsigned int	mKind;
			unsigned int	mTypeCrc;
			unsigned int	mFieldCrc;
		};

		// True when this pair is new (and should warn); false when already warned.
		bool MarkSeen(unsigned int kind, unsigned int typeCrc, unsigned int fieldCrc, unsigned int& overflow);

		Seen			mSeen[kMaxSeen];
		unsigned int	mSeenCount = 0;
	};
}
