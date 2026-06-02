#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		class PropertyInspectorController
		{
		public:
			Json::Value BuildPropertyJson(const Json::Value& sceneRoot,
			                             const char*        selectionType,
			                             const char*        selectionId) const;
		};
	}
}
