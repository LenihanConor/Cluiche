#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		class SceneHierarchyController
		{
		public:
			Json::Value BuildHierarchyJson(const Json::Value& sceneRoot) const;
		};
	}
}
