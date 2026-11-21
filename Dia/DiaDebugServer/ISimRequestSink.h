#ifndef DIA_DEBUG_SERVER_I_SIM_REQUEST_SINK_H
#define DIA_DEBUG_SERVER_I_SIM_REQUEST_SINK_H

#include <DiaDebugServer/SimHostedTypes.h>

namespace Dia
{
	namespace DebugServer
	{
		// Where the DebugServer (Main thread) posts requests destined for the SimPU.
		class ISimRequestSink
		{
		public:
			virtual ~ISimRequestSink() {}

			// Returns false if the stream is full.
			virtual bool PostRequest(const SimRequestEvent& request) = 0;
		};
	}
}

#endif
