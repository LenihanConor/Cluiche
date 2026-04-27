#pragma once

#include <DiaCore/Json/external/json/json.h>
#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace PipelineEditor
	{
		namespace Internal
		{
			inline Json::Value ParsePipelineTargets(const char* tomlPath)
			{
				Json::Value targets(Json::arrayValue);

				FILE* f = nullptr;
				if (fopen_s(&f, tomlPath, "r") != 0 || f == nullptr)
					return targets;

				char line[512];
				while (fgets(line, sizeof(line), f))
				{
					// Match [targets.NAME] but not [targets.NAME.package] etc.
					if (strncmp(line, "[targets.", 9) != 0)
						continue;

					const char* nameStart = line + 9;
					const char* nameEnd = strchr(nameStart, ']');
					if (nameEnd == nullptr)
						continue;

					// Skip sub-tables like [targets.NAME.package]
					const char* dot = strchr(nameStart, '.');
					if (dot != nullptr && dot < nameEnd)
						continue;

					size_t nameLen = static_cast<size_t>(nameEnd - nameStart);
					if (nameLen == 0 || nameLen >= 128)
						continue;

					char name[128];
					memcpy(name, nameStart, nameLen);
					name[nameLen] = '\0';

					targets.append(name);
				}

				fclose(f);
				return targets;
			}
		}
	}
}
