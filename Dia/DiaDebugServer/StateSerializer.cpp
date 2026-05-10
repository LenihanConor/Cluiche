#include "DiaDebugServer/StateSerializer.h"

#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

namespace Dia
{
	namespace DebugServer
	{
		Json::Value StateSerializer::SerializeProcessingUnitState(const Dia::Application::ProcessingUnit* pu)
		{
			Json::Value result;
			if (!pu)
			{
				result["error"] = "null processing unit";
				return result;
			}

			result["pu_id"] = pu->GetUniqueId().AsChar();
			result["state"] = pu->GetStateObjectType();

			const Dia::Application::Phase* currentPhase = pu->GetCurrentPhase();
			if (currentPhase)
			{
				result["current_phase"] = currentPhase->GetUniqueId().AsChar();
			}
			else
			{
				result["current_phase"] = Json::Value::null;
			}

			return result;
		}

		Json::Value StateSerializer::SerializePhaseState(const Dia::Application::Phase* phase)
		{
			Json::Value result;
			if (!phase)
			{
				result["error"] = "null phase";
				return result;
			}

			result["phase_id"] = phase->GetUniqueId().AsChar();
			result["state"] = phase->GetStateObjectType();

			return result;
		}

		Json::Value StateSerializer::SerializeModuleState(const Dia::Application::Module* module)
		{
			Json::Value result;
			if (!module)
			{
				result["error"] = "null module";
				return result;
			}

			result["module_id"] = module->GetUniqueId().AsChar();
			result["type"] = module->GetStateObjectType();
			result["started"] = module->HasStarted();

			return result;
		}

		Json::Value StateSerializer::SerializeCoreMetrics(const CoreMetrics& metrics)
		{
			Json::Value result;
			result["fps"] = metrics.fps;
			result["frame_time_ms"] = metrics.frameTimeMs;
			result["memory_used_mb"] = metrics.memoryUsedMb;
			result["memory_available_mb"] = metrics.memoryAvailableMb;
			return result;
		}

		Json::Value StateSerializer::SerializePhaseTransition(const Dia::Core::StringCRC& fromPhase,
		                                                      const Dia::Core::StringCRC& toPhase,
		                                                      uint64_t timestamp)
		{
			Json::Value result;
			result["from_phase"] = fromPhase.AsChar();
			result["to_phase"] = toPhase.AsChar();
			result["timestamp"] = static_cast<Json::UInt64>(timestamp);
			return result;
		}

		Json::Value StateSerializer::SerializeCommandResponse(const Dia::Core::StringCRC& command,
		                                                      bool success,
		                                                      const char* message)
		{
			Json::Value result;
			result["command"] = command.AsChar();
			result["success"] = success;
			result["message"] = message ? message : "";
			return result;
		}
	}
}
