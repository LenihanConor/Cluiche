#include "DiaSceneEditor/SceneHierarchyController.h"
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia
{
	namespace SceneEditor
	{
		Json::Value SceneHierarchyController::BuildHierarchyJson(const Json::Value& sceneRoot) const
		{
			DIA_TRACE_ZONE("SceneHierarchyController::BuildHierarchyJson", Dia::Observation::Trace::Category::kNone);

			Json::Value result(Json::objectValue);

			if (!sceneRoot.isMember("scene2d"))
				return result;

			const Json::Value& scene = sceneRoot["scene2d"];

			result["layers"]   = scene.isMember("layers")   ? scene["layers"]   : Json::Value(Json::arrayValue);
			result["cameras"]  = scene.isMember("cameras")  ? scene["cameras"]  : Json::Value(Json::arrayValue);
			result["lights"]   = scene.isMember("lights")   ? scene["lights"]   : Json::Value(Json::arrayValue);
			result["entities"] = scene.isMember("entities") ? scene["entities"] : Json::Value(Json::arrayValue);

			return result;
		}
	}
}
