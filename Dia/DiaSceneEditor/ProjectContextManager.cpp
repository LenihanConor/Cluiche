#include "DiaSceneEditor/ProjectContextManager.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#endif

namespace
{
	// Returns the directory portion of path (no trailing separator).
	static void DirOf(const char* path, char* outDir, int outSize)
	{
		strncpy(outDir, path, outSize - 1);
		outDir[outSize - 1] = '\0';
		char* lastSep = nullptr;
		for (char* p = outDir; *p; ++p)
		{
			if (*p == '/' || *p == '\\')
				lastSep = p;
		}
		if (lastSep)
			*lastSep = '\0';
		else
			outDir[0] = '\0';
	}

	// Joins dir + "/" + rel into out (normalises separators to /).
	static void JoinPath(const char* dir, const char* rel, char* out, int outSize)
	{
		if (rel[0] == '/' || rel[0] == '\\' || (rel[1] == ':'))
		{
			strncpy(out, rel, outSize - 1);
			out[outSize - 1] = '\0';
		}
		else
		{
			snprintf(out, outSize, "%s/%s", dir, rel);
		}
		// Normalise backslash to forward slash
		for (char* p = out; *p; ++p)
		{
			if (*p == '\\') *p = '/';
		}
	}

	static bool ReadJson(const char* path, Json::Value& out, char* errBuf, int errBufSize)
	{
		std::ifstream f(path);
		if (!f.is_open())
		{
			if (errBuf && errBufSize > 0) snprintf(errBuf, errBufSize, "cannot open: %s", path);
			return false;
		}
		std::string content((std::istreambuf_iterator<char>(f)),
		                     std::istreambuf_iterator<char>());
		Json::CharReaderBuilder b;
		std::string parseErr;
		std::istringstream ss(content);
		if (!Json::parseFromStream(b, ss, &out, &parseErr))
		{
			if (errBuf && errBufSize > 0) snprintf(errBuf, errBufSize, "parse error in %s: %s", path, parseErr.c_str());
			return false;
		}
		return true;
	}
}

namespace Dia
{
	namespace SceneEditor
	{
		Json::Value ProjectContextManager::BuildStageListJson(const char* diagamePath) const
		{
			DIA_TRACE_ZONE("ProjectContextManager::BuildStageListJson", Dia::Observation::Trace::Category::kNone);

			Json::Value result(Json::arrayValue);
			if (!diagamePath || diagamePath[0] == '\0')
				return result;

			char gameDir[512];
			DirOf(diagamePath, gameDir, sizeof(gameDir));

			Json::Value gameRoot;
			char err[256] = {};
			if (!ReadJson(diagamePath, gameRoot, err, sizeof(err)))
			{
				DIA_LOG_WARNING("Editor", "ProjectContextManager: failed to read diagame '%s': %s", diagamePath, err);
				return result;
			}

			if (!gameRoot.isMember("imports") || !gameRoot["imports"].isArray())
			{
				DIA_LOG_WARNING("Editor", "ProjectContextManager: no imports array in '%s'", diagamePath);
				return result;
			}

			const Json::Value& imports = gameRoot["imports"];
			for (unsigned int i = 0; i < imports.size(); ++i)
			{
				const Json::Value& imp = imports[i];
				if (!imp.isMember("type") || imp["type"].asString() != "stage")
					continue;
				if (!imp.isMember("path") || !imp["path"].isString())
					continue;

				char stagePath[512];
				JoinPath(gameDir, imp["path"].asCString(), stagePath, sizeof(stagePath));

				// Read the .diastage to get its name and optional scene path
				Json::Value stageRoot;
				char stageErr[256] = {};
				if (!ReadJson(stagePath, stageRoot, stageErr, sizeof(stageErr)))
				{
					DIA_LOG_WARNING("Editor", "ProjectContextManager: failed to read stage '%s': %s", stagePath, stageErr);
					continue;
				}

				const char* stageName = stageRoot.isMember("name") && stageRoot["name"].isString()
					? stageRoot["name"].asCString()
					: imp["path"].asCString();

				char stageDir[512];
				DirOf(stagePath, stageDir, sizeof(stageDir));

				char scenePath[512] = {};
				if (stageRoot.isMember("scene") && stageRoot["scene"].isString())
					JoinPath(stageDir, stageRoot["scene"].asCString(), scenePath, sizeof(scenePath));

				Json::Value entry(Json::objectValue);
				entry["name"]      = stageName;
				entry["stagePath"] = stagePath;
				entry["scenePath"] = scenePath;
				result.append(entry);
			}

			DIA_LOG_INFO("Editor", "ProjectContextManager: found %u stages in '%s'",
				result.size(), diagamePath);
			return result;
		}

		Json::Value ProjectContextManager::LoadScene(const char* scenePath, char* errBuf, int errBufSize) const
		{
			DIA_TRACE_ZONE("ProjectContextManager::LoadScene", Dia::Observation::Trace::Category::kNone);

			Json::Value root;
			if (!scenePath || scenePath[0] == '\0')
			{
				if (errBuf && errBufSize > 0) strncpy(errBuf, "empty scene path", errBufSize - 1);
				return root;
			}
			ReadJson(scenePath, root, errBuf, errBufSize);
			return root;
		}
	}
}
