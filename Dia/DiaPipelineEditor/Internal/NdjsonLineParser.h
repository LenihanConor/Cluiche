#pragma once

#include "DiaPipelineEditor/PipelineEvent.h"
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace PipelineEditor
	{
		namespace Internal
		{
			struct ParsedLine
			{
				PipelineEvent event;
				bool hasSchema = false;
				char schema[32] = {};
				bool hasTarget = false;
				char target[64] = {};
				bool hasConfig = false;
				char config[64] = {};
			};

			inline bool ParseNdjsonLine(const char* line, size_t len, ParsedLine& out)
			{
				Json::Reader reader;
				Json::Value root;

				if (!reader.parse(line, line + len, root, false))
					return false;

				if (!root.isObject())
					return false;

				if (!root.isMember("event") || !root.isMember("ts"))
					return false;

				out.event.eventType = Dia::Core::StringCRC(root["event"].asCString());
				out.event.timestampSec = static_cast<float>(root["ts"].asDouble());

				if (root.isMember("system"))
					out.event.system = Dia::Core::StringCRC(root["system"].asCString());

				if (root.isMember("stage"))
					out.event.stage = Dia::Core::StringCRC(root["stage"].asCString());

				if (root.isMember("step"))
					out.event.step = Dia::Core::StringCRC(root["step"].asCString());

				if (root.isMember("durationMs"))
					out.event.durationMs = root["durationMs"].asInt();

				if (root.isMember("schema"))
				{
					out.hasSchema = true;
					strncpy_s(out.schema, sizeof(out.schema), root["schema"].asCString(), _TRUNCATE);
				}

				if (root.isMember("target"))
				{
					out.hasTarget = true;
					strncpy_s(out.target, sizeof(out.target), root["target"].asCString(), _TRUNCATE);
				}

				if (root.isMember("config"))
				{
					out.hasConfig = true;
					strncpy_s(out.config, sizeof(out.config), root["config"].asCString(), _TRUNCATE);
				}

				return true;
			}
		}
	}
}
