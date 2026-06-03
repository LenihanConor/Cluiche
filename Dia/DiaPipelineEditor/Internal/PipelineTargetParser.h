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
			inline Json::Value ParseTargetStages(const char* tomlPath, const char* targetName)
			{
				Json::Value stages(Json::arrayValue);

				FILE* f = nullptr;
				if (fopen_s(&f, tomlPath, "r") != 0 || f == nullptr)
					return stages;

				bool inTarget = false;
				char line[512];

				while (fgets(line, sizeof(line), f))
				{
					if (line[0] == '[')
					{
						// If we were already in the matching target, a new section means we're done
						if (inTarget)
							break;

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

						if (strncmp(nameStart, targetName, nameLen) == 0 && targetName[nameLen] == '\0')
							inTarget = true;

						continue;
					}

					if (!inTarget)
						continue;

					// Look for:  stages = ["a", "b", "c"]
					const char* p = line;
					while (*p == ' ' || *p == '\t') ++p;
					if (strncmp(p, "stages", 6) != 0)
						continue;

					p += 6;
					while (*p == ' ' || *p == '\t') ++p;
					if (*p != '=')
						continue;

					++p; // skip '='

					// Walk through the rest of the line extracting quoted strings
					while (*p != '\0')
					{
						const char* q = strchr(p, '"');
						if (q == nullptr)
							break;

						const char* qEnd = strchr(q + 1, '"');
						if (qEnd == nullptr)
							break;

						size_t len = static_cast<size_t>(qEnd - (q + 1));
						if (len > 0)
						{
							char stageName[128] = {};
							if (len < sizeof(stageName))
							{
								memcpy(stageName, q + 1, len);
								stageName[len] = '\0';
								stages.append(stageName);
							}
						}

						p = qEnd + 1;
					}

					break; // stages line found and parsed
				}

				fclose(f);
				return stages;
			}
		}
	}
}
