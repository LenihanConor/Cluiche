#include "DiaSceneEditor/SceneValidator.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace SceneEditor
	{
		const char* SceneValidator::ExtractId(const Json::Value& val, char* buf, int bufSize)
		{
			if (val.isString())
			{
				strncpy(buf, val.asCString(), bufSize - 1);
				buf[bufSize - 1] = '\0';
				return buf;
			}
			if (val.isObject() && val.isMember("value") && val["value"].isString())
			{
				strncpy(buf, val["value"].asCString(), bufSize - 1);
				buf[bufSize - 1] = '\0';
				return buf;
			}
			buf[0] = '\0';
			return buf;
		}

		static Json::Value MakeIssue(const char* code, const char* message,
		                              const char* itemType = "", const char* itemId = "")
		{
			Json::Value v(Json::objectValue);
			v["code"]     = code;
			v["message"]  = message;
			v["itemType"] = itemType ? itemType : "";
			v["itemId"]   = itemId   ? itemId   : "";
			return v;
		}

		void SceneValidator::CheckActiveCameraCount(const Json::Value& scene,
		                                             Json::Value& errors) const
		{
			if (!scene.isMember("cameras") || !scene["cameras"].isArray()) return;

			const Json::Value& cameras = scene["cameras"];
			int activeCount = 0;
			for (unsigned int i = 0; i < cameras.size(); ++i)
			{
				if (cameras[i].isMember("active") && cameras[i]["active"].asBool())
					++activeCount;
			}

			if (activeCount == 0)
				errors.append(MakeIssue("NO_ACTIVE_CAMERA",
					"Scene must have exactly one active camera — none found",
					"camera", ""));
			else if (activeCount > 1)
			{
				char msg[256];
				snprintf(msg, sizeof(msg),
					"Scene must have exactly one active camera — %d found", activeCount);
				errors.append(MakeIssue("MULTIPLE_ACTIVE_CAMERAS", msg, "camera", ""));
			}
		}

		void SceneValidator::CheckDefaultLayerPresent(const Json::Value& scene,
		                                               Json::Value& warnings) const
		{
			if (!scene.isMember("layers") || !scene["layers"].isArray()) return;

			const Json::Value& layers = scene["layers"];
			if (layers.size() == 0)
			{
				warnings.append(MakeIssue("NO_LAYERS",
					"Scene has no layers — add at least one layer", "layer", ""));
			}
		}

		void SceneValidator::CheckUniqueIds(const Json::Value& scene, Json::Value& errors) const
		{
			// Check uniqueness within each section, and also globally across all sections.
			static const char* kSections[] = { "layers", "cameras", "lights", "entities" };
			static const char* kTypes[]    = { "layer",  "camera",  "light",  "entity"   };
			static const int   kCount      = 4;

			// Global id set (simple O(n²) — scenes are small)
			struct IdEntry { char id[256]; const char* type; };
			static const int kMaxIds = 256;
			IdEntry seen[kMaxIds];
			int seenCount = 0;

			char idBuf[256];
			for (int si = 0; si < kCount; ++si)
			{
				if (!scene.isMember(kSections[si]) || !scene[kSections[si]].isArray()) continue;
				const Json::Value& arr = scene[kSections[si]];

				for (unsigned int i = 0; i < arr.size() && seenCount < kMaxIds; ++i)
				{
					if (!arr[i].isMember("id")) continue;
					const char* id = ExtractId(arr[i]["id"], idBuf, sizeof(idBuf));
					if (id[0] == '\0')
					{
						char msg[256];
						snprintf(msg, sizeof(msg), "Empty id in %s at index %u", kSections[si], i);
						errors.append(MakeIssue("EMPTY_ID", msg, kTypes[si], ""));
						continue;
					}

					// Check for duplicate
					for (int j = 0; j < seenCount; ++j)
					{
						if (strcmp(seen[j].id, id) == 0)
						{
							char msg[256];
							snprintf(msg, sizeof(msg),
								"Duplicate id '%s' — also used in %s", id, seen[j].type);
							errors.append(MakeIssue("DUPLICATE_ID", msg, kTypes[si], id));
							break;
						}
					}

					strncpy(seen[seenCount].id, id, 255);
					seen[seenCount].id[255] = '\0';
					seen[seenCount].type = kTypes[si];
					++seenCount;
				}
			}
		}

		void SceneValidator::CheckLayerReferences(const Json::Value& scene,
		                                            Json::Value& warnings) const
		{
			if (!scene.isMember("layers") || !scene["layers"].isArray()) return;
			if (!scene.isMember("lights") || !scene["lights"].isArray()) return;

			// Build set of valid layer ids
			char layerIds[32][256];
			int layerCount = 0;
			char buf[256];
			const Json::Value& layers = scene["layers"];
			for (unsigned int i = 0; i < layers.size() && layerCount < 32; ++i)
			{
				if (layers[i].isMember("id"))
				{
					strncpy(layerIds[layerCount], ExtractId(layers[i]["id"], buf, sizeof(buf)), 255);
					layerIds[layerCount][255] = '\0';
					++layerCount;
				}
			}

			const Json::Value& lights = scene["lights"];
			for (unsigned int i = 0; i < lights.size(); ++i)
			{
				if (!lights[i].isMember("affects_layers")) continue;
				const Json::Value& al = lights[i]["affects_layers"];
				char lightId[256];
				ExtractId(lights[i].isMember("id") ? lights[i]["id"] : Json::Value(""),
				          lightId, sizeof(lightId));

				for (unsigned int j = 0; j < al.size(); ++j)
				{
					char refId[256];
					ExtractId(al[j], refId, sizeof(refId));
					bool valid = false;
					for (int li = 0; li < layerCount; ++li)
						if (strcmp(layerIds[li], refId) == 0) { valid = true; break; }
					if (!valid)
					{
						char msg[256];
						snprintf(msg, sizeof(msg),
							"Light '%s' references unknown layer '%s'", lightId, refId);
						warnings.append(MakeIssue("UNKNOWN_LAYER_REF", msg, "light", lightId));
					}
				}
			}
		}

		Json::Value SceneValidator::Validate(const Json::Value& sceneRoot) const
		{
			DIA_TRACE_ZONE("SceneValidator::Validate", Dia::Observation::Trace::Category::kNone);

			Json::Value result(Json::objectValue);
			Json::Value errors(Json::arrayValue);
			Json::Value warnings(Json::arrayValue);

			if (!sceneRoot.isMember("scene2d"))
			{
				errors.append(MakeIssue("MISSING_SCENE2D", "scene2d root key not found"));
				result["valid"]    = false;
				result["errors"]   = errors;
				result["warnings"] = warnings;
				return result;
			}

			const Json::Value& scene = sceneRoot["scene2d"];

			CheckActiveCameraCount(scene, errors);
			CheckDefaultLayerPresent(scene, warnings);
			CheckUniqueIds(scene, errors);
			CheckLayerReferences(scene, warnings);

			if (errors.size() > 0)
				DIA_LOG_WARNING("Editor", "SceneValidator: %u error(s) found", errors.size());
			if (warnings.size() > 0)
				DIA_LOG_INFO("Editor", "SceneValidator: %u warning(s) found", warnings.size());

			result["valid"]    = errors.size() == 0;
			result["errors"]   = errors;
			result["warnings"] = warnings;
			return result;
		}
	}
}
