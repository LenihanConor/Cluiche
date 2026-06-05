#include "DiaSceneEditor/SceneHierarchyController.h"
#include <DiaObservation/Trace/DiaTrace.h>
#include <cstring>
#include <cctype>

namespace
{
	// Case-insensitive substring search (needle in haystack).
	static bool ContainsCI(const char* haystack, const char* needle)
	{
		if (!needle || needle[0] == '\0') return true;
		if (!haystack) return false;
		for (const char* h = haystack; *h; ++h)
		{
			const char* hi = h;
			const char* ni = needle;
			while (*hi && *ni && std::tolower((unsigned char)*hi) == std::tolower((unsigned char)*ni))
			{ ++hi; ++ni; }
			if (*ni == '\0') return true;
		}
		return false;
	}
}

namespace Dia
{
	namespace SceneEditor
	{
		// static
		const char* SceneHierarchyController::ExtractId(const Json::Value& val, char* buf, int bufSize)
		{
			if (val.isString())
			{
				strncpy_s(buf, bufSize, val.asCString(), _TRUNCATE);
				return buf;
			}
			if (val.isObject() && val.isMember("value") && val["value"].isString())
			{
				strncpy_s(buf, bufSize, val["value"].asCString(), _TRUNCATE);
				return buf;
			}
			buf[0] = '\0';
			return buf;
		}

		Json::Value SceneHierarchyController::NormaliseSection(const Json::Value& arr,
		                                                        const char* sectionType,
		                                                        const char* filter) const
		{
			Json::Value result(Json::arrayValue);
			if (!arr.isArray()) return result;

			char idBuf[256];
			for (unsigned int i = 0; i < arr.size(); ++i)
			{
				const Json::Value& item = arr[i];

				// Extract id
				const char* id = "";
				if (item.isMember("id"))
					id = ExtractId(item["id"], idBuf, sizeof(idBuf));

				// Extract label — use "name" if present, else fall back to id
				char labelBuf[256];
				const char* label = id;
				if (item.isMember("name"))
				{
					const char* n = ExtractId(item["name"], labelBuf, sizeof(labelBuf));
					if (n && n[0] != '\0') label = n;
				}

				// Filter check
				if (filter && filter[0] != '\0')
				{
					if (!ContainsCI(id, filter) && !ContainsCI(label, filter))
						continue;
				}

				bool enabled = true;
				if (item.isMember("enabled") && item["enabled"].isBool())
					enabled = item["enabled"].asBool();

				Json::Value entry(Json::objectValue);
				entry["id"]      = id;
				entry["label"]   = label;
				entry["type"]    = sectionType;
				entry["enabled"] = enabled;

				// Preserve entity template for cameras/lights/entities
				if (item.isMember("blueprint"))
				{
					char etBuf[256];
					entry["entityTemplate"] = ExtractId(item["blueprint"], etBuf, sizeof(etBuf));
				}

				// Active flag for cameras
				if (item.isMember("active") && item["active"].isBool())
					entry["active"] = item["active"].asBool();

				result.append(entry);
			}
			return result;
		}

		Json::Value SceneHierarchyController::BuildHierarchyJson(const Json::Value& sceneRoot) const
		{
			DIA_TRACE_ZONE("SceneHierarchyController::BuildHierarchyJson", Dia::Observation::Trace::Category::kNone);
			return BuildFilteredHierarchyJson(sceneRoot, nullptr);
		}

		Json::Value SceneHierarchyController::BuildFilteredHierarchyJson(const Json::Value& sceneRoot,
		                                                                   const char* filter) const
		{
			DIA_TRACE_ZONE("SceneHierarchyController::BuildFilteredHierarchyJson", Dia::Observation::Trace::Category::kNone);

			Json::Value result(Json::objectValue);

			if (!sceneRoot.isMember("scene2d"))
				return result;

			const Json::Value& scene = sceneRoot["scene2d"];

			static const Json::Value kEmpty(Json::arrayValue);
			result["layers"]   = NormaliseSection(scene.isMember("layers")   ? scene["layers"]   : kEmpty, "layer",  filter);
			result["cameras"]  = NormaliseSection(scene.isMember("cameras")  ? scene["cameras"]  : kEmpty, "camera", filter);
			result["lights"]   = NormaliseSection(scene.isMember("lights")   ? scene["lights"]   : kEmpty, "light",  filter);
			result["entities"] = NormaliseSection(scene.isMember("entities") ? scene["entities"] : kEmpty, "entity", filter);

			result["selection"] = GetSelectionJson();

			return result;
		}

		void SceneHierarchyController::SetSelection(const char* type, const char* id)
		{
			strncpy_s(mSelectionType, sizeof(mSelectionType), type ? type : "", _TRUNCATE);
			strncpy_s(mSelectionId,   sizeof(mSelectionId),   id   ? id   : "", _TRUNCATE);
		}

		void SceneHierarchyController::ClearSelection()
		{
			mSelectionType[0] = '\0';
			mSelectionId[0]   = '\0';
		}

		Json::Value SceneHierarchyController::GetSelectionJson() const
		{
			Json::Value sel(Json::objectValue);
			sel["type"] = mSelectionType;
			sel["id"]   = mSelectionId;
			return sel;
		}
	}
}
