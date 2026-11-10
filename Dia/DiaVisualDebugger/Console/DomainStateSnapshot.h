////////////////////////////////////////////////////////////////////////////////
// Filename: DomainStateSnapshot.h
// Description: Fixed-size POD carrying every domain's panel state JSON from the
//              producer (SimPU) to the consumer (Main) over a FrameStream, plus the
//              pure packer that fills it. The JSON is exactly what
//              BuildDomainStatesRoot produces (keyed by display name), serialised
//              compact. Packing is whole-domain: a domain that does not fit in the
//              budget is dropped entirely and the truncated flag is set, so the
//              JSON in the snapshot always parses.
//              The packer does no logging or metrics; the caller reports the result.
// Feature spec: docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>

#include <cstdint>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			static constexpr uint32_t kMaxSnapshotBytes = 128 * 1024;

			static constexpr uint32_t kSnapshotFlagTruncated          = 1u << 0; // at least one domain dropped for budget
			static constexpr uint32_t kSnapshotFlagStaleDomainSkipped = 1u << 1; // reserved for the producer (domain skipped as stale)

			// ~128 KB: heap-allocate, never place on the stack.
			struct DomainStateSnapshot
			{
				uint64_t simFrame = 0;
				uint32_t domainCount = 0;   // domains included in json
				uint32_t jsonBytes = 0;     // length of json (a terminating NUL is written only if there is room)
				uint32_t flags = 0;
				char     json[kMaxSnapshotBytes];
			};

			struct PackResult
			{
				bool     truncated = false;
				uint32_t includedCount = 0;
				uint32_t droppedCount = 0;
			};

			// Overwrites out with a JSON object {"<displayName>": {state...}, ...} (same per-domain
			// content as BuildDomainStatesRoot). Total JSON length never exceeds
			// min(maxBytes, kMaxSnapshotBytes). Domains with a null display name are skipped
			// (not counted as dropped). Domains are tried in VisitAll order; a domain that does not
			// fit is dropped and later, smaller ones may still be included.
			PackResult PackDomainStates(const DiaDebugDomainRegistry& registry, const ISchemaKeyResolver* resolver,
									   uint64_t simFrame, DomainStateSnapshot& out,
									   uint32_t maxBytes = kMaxSnapshotBytes);
		}
	}
}

#endif // DIA_DEBUG
