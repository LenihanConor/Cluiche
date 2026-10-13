////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleLineAssembler.cpp
// Description: Joins JS bridge chunks into one console line.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/ConsoleLineAssembler.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			bool ConsoleLineAssembler::Assemble(const char* const* chunks, unsigned int count, char* out, unsigned int outSize)
			{
				if (out == nullptr || outSize == 0)
				{
					return false;
				}
				out[0] = '\0';

				if (count > kMaxChunks || (count > 0 && chunks == nullptr))
				{
					return false;
				}

				unsigned int used = 0;
				for (unsigned int c = 0; c < count; ++c)
				{
					const char* chunk = chunks[c];
					if (chunk == nullptr)
					{
						continue;
					}

					for (unsigned int i = 0; chunk[i] != '\0'; ++i)
					{
						if (i >= kMaxChunkLength || used >= kMaxLineLength || used + 1 >= outSize)
						{
							out[0] = '\0';
							return false;
						}
						out[used++] = chunk[i];
					}
				}

				out[used] = '\0';
				return true;
			}
		}
	}
}
