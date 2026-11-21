////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleLineAssembler.h
// Description: Joins the chunks a console line is split into by the JS bridge
//              (Ultralight marshals each string argument through 63 characters)
//              back into one line of at most kMaxCommandLineLength characters.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			class ConsoleLineAssembler
			{
			public:
				static const unsigned int kMaxChunks = 4;
				static const unsigned int kMaxChunkLength = 63;		// characters, excluding '\0'
				static const unsigned int kMaxLineLength = 240;		// characters, excluding '\0'

				////////////////////////////////////////////////////////////////////////////////
				// Concatenate chunks[0..count) into out.
				//   - nullptr chunks are treated as empty.
				//   - Returns false (and writes "" to out when outSize > 0) if count exceeds
				//     kMaxChunks, a chunk exceeds kMaxChunkLength, the joined line exceeds
				//     kMaxLineLength, or out cannot hold the line plus '\0'.
				////////////////////////////////////////////////////////////////////////////////
				static bool Assemble(const char* const* chunks, unsigned int count, char* out, unsigned int outSize);
			};
		}
	}
}
