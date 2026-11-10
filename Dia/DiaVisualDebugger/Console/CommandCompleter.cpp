////////////////////////////////////////////////////////////////////////////////
// Filename: CommandCompleter.cpp
// Description: Completes console command names against the live registry.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/CommandCompleter.h>

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>

#include <cstring>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			unsigned CommandCompleter::Complete(const char* text, Completion* out, unsigned maxOut)
			{
				if (!text || !out || maxOut == 0 || std::strchr(text, ' ') != nullptr)
				{
					return 0;
				}

				Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfo, 256> infos;
				Dia::API::ListCommandInfos(infos);

				const unsigned count = static_cast<unsigned>(infos.Size());
				if (count == 0)
				{
					return 0;
				}

				const char* names[256];
				const unsigned n = count < 256 ? count : 256;
				for (unsigned i = 0; i < n; ++i)
				{
					names[i] = infos[i].name.AsChar();
					if (!names[i]) names[i] = "";
				}

				unsigned indices[256];
				const unsigned written = FuzzyMatcher::Rank(text, names, n, indices, maxOut < 256 ? maxOut : 256);
				for (unsigned i = 0; i < written; ++i)
				{
					const char* name = names[indices[i]];
					std::strncpy(out[i].name, name, sizeof(out[i].name) - 1);
					out[i].name[sizeof(out[i].name) - 1] = '\0';
					out[i].rank = FuzzyMatcher::Match(text, name, nullptr, 0);
				}
				return written;
			}
		}
	}
}
