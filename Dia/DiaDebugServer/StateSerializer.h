#ifndef DIA_DEBUG_SERVER_STATE_SERIALIZER_H
#define DIA_DEBUG_SERVER_STATE_SERIALIZER_H

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia
{
	namespace ApplicationFlow
	{
		class IApplicationInspectable;
		struct ModuleStateInfo;
	}

	namespace DebugServer
	{
		// Frame-rate and memory snapshot broadcast over CORE_METRICS messages.
		// Populated by DebugServerModule itself in v2 (no separate collector
		// module — per-PU timings are dropped; frame time comes from DoUpdate
		// deltaTime and memory comes from the OS).
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
			// Serialize the running Application's current stage + active modules.
			// Null app → {"error": "null application"}.
			static Json::Value SerializeApplicationState(const Dia::ApplicationFlow::IApplicationInspectable* app);

			// Serialize a single module's state snapshot (from GetActiveModules).
			static Json::Value SerializeModuleState(const Dia::ApplicationFlow::ModuleStateInfo& info);

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
