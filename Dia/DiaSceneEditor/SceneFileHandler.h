#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		class SceneFileHandler
		{
		public:
			bool Load(const char* path, Json::Value& outRoot, char* errBuf, int errBufSize);
			bool Save(const char* path, const Json::Value& root, char* errBuf, int errBufSize);
		};
	}
}
