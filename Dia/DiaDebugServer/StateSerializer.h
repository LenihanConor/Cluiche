#ifndef DIA_DEBUG_SERVER_STATE_SERIALIZER_H
#define DIA_DEBUG_SERVER_STATE_SERIALIZER_H

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia
{
	namespace DebugServer
	{
		class IDebugStateProvider;
		struct DebugModuleInfo;

		// Frame-rate and memory snapshot broadcast over CORE_METRICS messages.
		// Populated by DebugServer's host adapter — per-PU timings are not
		// tracked here; frame time comes from the host tick delta and memory
		// comes from the OS.
		struct CoreMetrics
		{
			float fps;
			float frameTimeMs;
			float memoryUsedMb;
			float memoryAvailableMb;
		};

		class StateSerializer
		{
		public:
			// Serialize the host application's current stage + active modules.
			// Null provider → {"error": "null state provider"}.
			static Json::Value SerializeApplicationState(const IDebugStateProvider* provider);

			// Serialize a single module snapshot.
			static Json::Value SerializeModuleState(const DebugModuleInfo& info);

			// Serialize a stage transition event.
			static Json::Value SerializeStageTransition(const Dia::Core::StringCRC& fromStage,
			                                            const Dia::Core::StringCRC& toStage,
			                                            uint64_t timestamp);

			static Json::Value SerializeCoreMetrics(const CoreMetrics& metrics);

			static Json::Value SerializeCommandResponse(const Dia::Core::StringCRC& command,
			                                            bool success,
			                                            const char* message);
		};
	}
}

#endif // DIA_DEBUG_SERVER_STATE_SERIALIZER_H
