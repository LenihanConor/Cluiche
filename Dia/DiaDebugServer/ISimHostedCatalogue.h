#ifndef DIA_DEBUG_SERVER_I_SIM_HOSTED_CATALOGUE_H
#define DIA_DEBUG_SERVER_I_SIM_HOSTED_CATALOGUE_H

////////////////////////////////////////////////////////////////////////////////
// Filename: ISimHostedCatalogue.h
//
// Read-only view of the Main-side sim-hosted catalogue (names registered on SimQueryHost together with their
// help).  Implemented by DebugServer.  Consumers of command help (palette index, schema collector, command list)
// receive a pointer to it by injection; there is no static accessor.
//
// Main thread only: the catalogue is Main-owned, so an implementation is NOT safe to read from another
// processing unit.  Pointers handed out (SimHostedEntryView::help, FindSimHostedHelp) stay valid only until the
// next register/unregister/replace on the owner; copy what you need to keep.
////////////////////////////////////////////////////////////////////////////////

#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/SimHostedTypes.h>

#include <cstdint>

namespace Dia
{
	namespace DebugServer
	{
		class ISimHostedCatalogue
		{
		public:
			virtual ~ISimHostedCatalogue() = default;

			// Entries are in registration order.
			virtual uint32_t SimHostedCount() const = 0;
			// False if index >= SimHostedCount().
			virtual bool GetSimHostedEntry(uint32_t index, SimHostedEntryView& out) const = 0;
			// Null if 'name' is not sim-hosted.
			virtual const SimHelpStored* FindSimHostedHelp(Dia::Core::StringCRC name) const = 0;
		};

		// Fills 'out' (capacity kSimHelpMaxParams) with ValueParam views over the stored help's strings, in the shape
		// DiaAPI commands use.  Empty optional strings become nullptr.  The views point into 'help' and are valid
		// exactly as long as 'help' is.  Returns the number of params written.
		inline uint32_t SimHelpToParams(const SimHelpStored& help, Dia::API::ValueParam* out)
		{
			auto text = [](const char* s) -> const char* { return (s != nullptr && s[0] != '\0') ? s : nullptr; };
			const uint32_t count = help.paramCount < kSimHelpMaxParams ? help.paramCount : kSimHelpMaxParams;
			for (uint32_t i = 0; i < count; ++i)
			{
				const SimParamStored& p = help.params[i];
				Dia::API::ValueParam v;
				v.name        = text(p.name);
				v.type        = text(p.type);
				v.required    = p.required;
				v.description = text(p.description);
				v.unit        = text(p.unit);
				v.hasRange    = p.hasRange;
				v.min         = p.min;
				v.max         = p.max;
				v.enumValues  = text(p.enumValues);
				out[i] = v;
			}
			return count;
		}
	}
}

#endif
