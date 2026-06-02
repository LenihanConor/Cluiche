#include "DiaSceneEditor/PropertyInspectorController.h"
#include <DiaObservation/Trace/DiaTrace.h>
#include <cstring>

namespace Dia
{
	namespace SceneEditor
	{
		Json::Value PropertyInspectorController::BuildPropertyJson(const Json::Value& sceneRoot,
		                                                           const char*        selectionType,
		                                                           const char*        selectionId) const
		{
			DIA_TRACE_ZONE("PropertyInspectorController::BuildPropertyJson", Dia::Observation::Trace::Category::kNone);

			Json::Value result(Json::objectValue);

			if (!sceneRoot.isMember("scene2d") || !selectionType || !selectionId)
				return result;

			const Json::Value& scene = sceneRoot["scene2d"];

			const char* arrayKey = nullptr;
			if (strcmp(selectionType, "entity")  == 0) arrayKey = "entities";
			else if (strcmp(selectionType, "camera") == 0) arrayKey = "cameras";
			else if (strcmp(selectionType, "light")  == 0) arrayKey = "lights";
			else if (strcmp(selectionType, "layer")  == 0) arrayKey = "layers";

			if (!arrayKey || !scene.isMember(arrayKey))
				return result;

			const Json::Value& arr = scene[arrayKey];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				const Json::Value& item = arr[i];
				if (item.isMember("id") && item["id"].asString() == selectionId)
				{
					result = item;
					result["selectionType"] = selectionType;
					return result;
				}
			}

			return result;
		}
	}
}
