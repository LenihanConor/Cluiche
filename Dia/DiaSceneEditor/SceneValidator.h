#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		// T20: Scene validation — enforces structural constraints on a .diascene.
		// Returns a JSON report used by the UI to show errors/warnings and block save.
		class SceneValidator
		{
		public:
			// Returns:
			// {
			//   valid: bool,       -- false if any errors present (save should be blocked)
			//   errors:   [...],   -- { code, message, itemType, itemId }
			//   warnings: [...],   -- same structure, non-blocking
			// }
			Json::Value Validate(const Json::Value& sceneRoot) const;

		private:
			static const char* ExtractId(const Json::Value& val, char* buf, int bufSize);

			void CheckActiveCameraCount(const Json::Value& scene, Json::Value& errors) const;
			void CheckDefaultLayerPresent(const Json::Value& scene, Json::Value& warnings) const;
			void CheckUniqueIds(const Json::Value& scene, Json::Value& errors) const;
			void CheckLayerReferences(const Json::Value& scene, Json::Value& warnings) const;
		};
	}
}
