#include "DiaSceneEditor/SceneFileHandler.h"
#include <DiaObservation/Log/DiaLog.h>
#include <fstream>
#include <sstream>
#include <cstring>

namespace Dia
{
	namespace SceneEditor
	{
		bool SceneFileHandler::Load(const char* path, Json::Value& outRoot, char* errBuf, int errBufSize)
		{
			if (!path || path[0] == '\0')
			{
				if (errBuf && errBufSize > 0) { strncpy(errBuf, "empty path", errBufSize - 1); errBuf[errBufSize - 1] = '\0'; }
				return false;
			}

			std::ifstream file(path);
			if (!file.is_open())
			{
				if (errBuf && errBufSize > 0)
				{
					snprintf(errBuf, errBufSize, "cannot open file: %s", path);
				}
				DIA_LOG_WARNING("Editor", "SceneFileHandler: cannot open '%s'", path);
				return false;
			}

			std::string content((std::istreambuf_iterator<char>(file)),
			                     std::istreambuf_iterator<char>());

			Json::CharReaderBuilder builder;
			std::string parseErr;
			std::istringstream ss(content);

			if (!Json::parseFromStream(builder, ss, &outRoot, &parseErr))
			{
				if (errBuf && errBufSize > 0)
				{
					snprintf(errBuf, errBufSize, "JSON parse error: %s", parseErr.c_str());
				}
				DIA_LOG_WARNING("Editor", "SceneFileHandler: parse error in '%s': %s", path, parseErr.c_str());
				return false;
			}

			return true;
		}

		bool SceneFileHandler::Save(const char* path, const Json::Value& root, char* errBuf, int errBufSize)
		{
			if (!path || path[0] == '\0')
			{
				if (errBuf && errBufSize > 0) { strncpy(errBuf, "empty path", errBufSize - 1); errBuf[errBufSize - 1] = '\0'; }
				return false;
			}

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			std::string output     = Json::writeString(builder, root);

			std::ofstream file(path);
			if (!file.is_open())
			{
				if (errBuf && errBufSize > 0)
				{
					snprintf(errBuf, errBufSize, "cannot open file for writing: %s", path);
				}
				DIA_LOG_WARNING("Editor", "SceneFileHandler: cannot write '%s'", path);
				return false;
			}

			file << output;
			return true;
		}
	}
}
