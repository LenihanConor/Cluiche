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

				char currentTarget[128] = {};
				bool currentHidden = false;
				char line[512];

				while (fgets(line, sizeof(line), f))
				{
					if (line[0] == '[')
					{
						// Commit previous target if not hidden
						if (currentTarget[0] != '\0' && !currentHidden)
							targets.append(currentTarget);

						currentTarget[0] = '\0';
						currentHidden = false;

						// Match [targets.NAME] but not [targets.NAME.subtable]
						if (strncmp(line, "[targets.", 9) != 0)
							continue;

						const char* nameStart = line + 9;
						const char* nameEnd = strchr(nameStart, ']');
						if (nameEnd == nullptr)
							continue;

						const char* dot = strchr(nameStart, '.');
						if (dot != nullptr && dot < nameEnd)
							continue;

						size_t nameLen = static_cast<size_t>(nameEnd - nameStart);
						if (nameLen == 0 || nameLen >= 128)
							continue;

						memcpy(currentTarget, nameStart, nameLen);
						currentTarget[nameLen] = '\0';
						continue;
					}

					if (currentTarget[0] != '\0' && strncmp(line, "hidden", 6) == 0)
					{
						const char* eq = strchr(line, '=');
						if (eq && strstr(eq, "true"))
							currentHidden = true;
					}
				}

				// Commit last target
				if (currentTarget[0] != '\0' && !currentHidden)
					targets.append(currentTarget);

				fclose(f);
				return targets;
			}
		}
	}
}
