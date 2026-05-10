#include "DiaDebugServer/StateSerializer.h"
#include "DiaDebugServer/IDebugStateProvider.h"

namespace Dia
{
	namespace DebugServer
	{
		Json::Value StateSerializer::SerializeApplicationState(const IDebugStateProvider* provider)
		{
			Json::Value result;
			if (!provider)
			{
				result["error"] = "null state provider";
				return result;
			}

			result["current_stage"]    = provider->GetCurrentStage().AsChar();
			result["is_transitioning"] = provider->IsTransitioning();
			result["is_shutting_down"] = provider->IsShuttingDown();

			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
			provider->GetProcessingUnitIds(puIds);

			Json::Value pusJson(Json::arrayValue);
			for (unsigned int p = 0; p < puIds.Size(); ++p)
			{
				Json::Value puJson;
				puJson["pu_id"] = puIds[p].AsChar();

				Dia::Core::Containers::DynamicArrayC<DebugModuleInfo, 64> modules;
				provider->GetModulesInPU(puIds[p], modules);

				Json::Value modulesJson(Json::arrayValue);
				for (unsigned int m = 0; m < modules.Size(); ++m)
				{
					modulesJson.append(SerializeModuleState(modules[m]));
				}
				puJson["modules"] = modulesJson;

				pusJson.append(puJson);
			}
			result["processing_units"] = pusJson;

			return result;
		}

		Json::Value StateSerializer::SerializeModuleState(const DebugModuleInfo& info)
		{
			Json::Value result;
			result["module_id"] = info.instanceId.AsChar();
			result["type_id"]   = info.typeId.AsChar();
			result["state"]     = info.state ? info.state : "unknown";
			return result;
		}

		Json::Value StateSerializer::SerializeStageTransition(const Dia::Core::StringCRC& fromStage,
		                                                      const Dia::Core::StringCRC& toStage,
		                                                      uint64_t timestamp)
		{
			Json::Value result;
			result["from_stage"] = fromStage.AsChar();
			result["to_stage"]   = toStage.AsChar();
			result["timestamp"]  = static_cast<Json::UInt64>(timestamp);
			return result;
		}

		Json::Value StateSerializer::SerializeCoreMetrics(const CoreMetrics& metrics)
		{
			Json::Value result;
			result["fps"]                  = metrics.fps;
			result["frame_time_ms"]        = metrics.frameTimeMs;
			result["memory_used_mb"]       = metrics.memoryUsedMb;
			result["memory_available_mb"]  = metrics.memoryAvailableMb;
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
