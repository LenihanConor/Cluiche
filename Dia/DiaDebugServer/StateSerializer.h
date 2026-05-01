#ifndef DIA_DEBUG_SERVER_STATE_SERIALIZER_H
#define DIA_DEBUG_SERVER_STATE_SERIALIZER_H

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class Phase;
		class Module;
	}

	namespace DebugServer
	{
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
			static Json::Value SerializeProcessingUnitState(const Dia::Application::ProcessingUnit* pu);
			static Json::Value SerializePhaseState(const Dia::Application::Phase* phase);
			static Json::Value SerializeModuleState(const Dia::Application::Module* module);
			static Json::Value SerializeCoreMetrics(const CoreMetrics& metrics);
			static Json::Value SerializePhaseTransition(const Dia::Core::StringCRC& fromPhase,
			                                            const Dia::Core::StringCRC& toPhase,
			                                            uint64_t timestamp);
			static Json::Value SerializeCommandResponse(const Dia::Core::StringCRC& command,
			                                            bool success,
			                                            const char* message);
		};
	}
}

#endif // DIA_DEBUG_SERVER_STATE_SERIALIZER_H
