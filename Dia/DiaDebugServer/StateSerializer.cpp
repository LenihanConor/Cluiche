#include "DiaDebugServer/StateSerializer.h"

#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace DebugServer
	{
		Json::Value StateSerializer::SerializeApplicationState(const Dia::ApplicationFlow::IApplicationInspectable* app)
		{
			Json::Value result;
			if (!app)
			{
				result["error"] = "null application";
				return result;
			}

			result["current_stage"] = app->GetCurrentStage().AsChar();
			result["is_transitioning"] = app->IsTransitioning();
			result["is_shutting_down"] = app->IsShuttingDown();

			// Enumerate PUs.
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
			app->GetProcessingUnits(puIds);

			Json::Value pusJson(Json::arrayValue);
			for (unsigned int p = 0; p < puIds.Size(); ++p)
			{
				Json::Value puJson;
				puJson["pu_id"] = puIds[p].AsChar();

				Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::ModuleStateInfo, 64> modules;
				app->GetActiveModules(puIds[p], modules);

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

		Json::Value StateSerializer::SerializeModuleState(const Dia::ApplicationFlow::ModuleStateInfo& info)
		{
			Json::Value result;
			result["module_id"] = info.instanceId.AsChar();
			result["type_id"]   = info.typeId.AsChar();

			const char* stateName = "unknown";
			switch (info.state)
			{
				case Dia::ApplicationFlow::ModuleState::kInactive: stateName = "inactive"; break;
				case Dia::ApplicationFlow::ModuleState::kStarting: stateName = "starting"; break;
				case Dia::ApplicationFlow::ModuleState::kActive:   stateName = "active";   break;
				case Dia::ApplicationFlow::ModuleState::kStopping: stateName = "stopping"; break;
				case Dia::ApplicationFlow::ModuleState::kFailed:   stateName = "failed";   break;
			}
			result["state"] = stateName;
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
