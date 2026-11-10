////////////////////////////////////////////////////////////////////////////////
// Filename: DomainStateSnapshot.cpp
// Description: See DomainStateSnapshot.h.
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/DomainStateSnapshot.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <string>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			PackResult PackDomainStates(const DiaDebugDomainRegistry& registry, const ISchemaKeyResolver* resolver,
										uint64_t simFrame, DomainStateSnapshot& out, uint32_t maxBytes)
			{
				const uint32_t budget = maxBytes < kMaxSnapshotBytes ? maxBytes : kMaxSnapshotBytes;

				PackResult result;
				out.simFrame = simFrame;
				out.domainCount = 0;
				out.flags = 0;

				// Written bytes so far, always starting with the opening brace.
				uint32_t used = 0;
				if (budget >= 2)
				{
					out.json[used++] = '{';
				}

				Json::FastWriter writer;
				registry.VisitAll([&](IDebugDomain& domain)
				{
					Json::Value state;
					if (!BuildDomainState(domain, state, resolver))
						return;

					// {"<name>":{...}}\n  ->  "<name>":{...}
					Json::Value wrapper(Json::objectValue);
					wrapper[domain.GetDisplayName()] = state;
					std::string text = writer.write(wrapper);
					while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
						text.pop_back();
					const size_t entryLen = text.size() - 2;   // drop outer braces

					const uint32_t separator = result.includedCount > 0 ? 1u : 0u;
					// Leave room for the closing brace.
					if (budget < 2 || static_cast<uint64_t>(used) + separator + entryLen + 1 > budget)
					{
						++result.droppedCount;
						return;
					}

					if (separator)
						out.json[used++] = ',';
					std::memcpy(out.json + used, text.data() + 1, entryLen);
					used += static_cast<uint32_t>(entryLen);
					++result.includedCount;
				});

				if (budget >= 2)
				{
					out.json[used++] = '}';
				}
				out.jsonBytes = used;
				if (used < kMaxSnapshotBytes)
					out.json[used] = '\0';

				out.domainCount = result.includedCount;
				if (result.droppedCount > 0)
				{
					result.truncated = true;
					out.flags |= kSnapshotFlagTruncated;
				}
				return result;
			}
		}
	}
}

#endif // DIA_DEBUG
