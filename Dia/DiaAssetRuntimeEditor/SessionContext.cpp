#include "DiaAssetRuntimeEditor/SessionContext.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <fstream>
#include <string>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			static const char* kContextFileName = "DiaAssetRuntimeEditor.context.json";
			static const float kDefaultPollInterval = 1.0f;
			static const unsigned int kDefaultMaxLogEntries = 1000;

			SessionContext::SessionContext()
				: mPollInterval(kDefaultPollInterval)
				, mMaxLogEntries(kDefaultMaxLogEntries)
			{}

			void SessionContext::Load(const char* outputDir)
			{
				mPollInterval = kDefaultPollInterval;
				mMaxLogEntries = kDefaultMaxLogEntries;

				std::string path = std::string(outputDir) + "/" + kContextFileName;
				std::ifstream file(path);
				if (!file.is_open())
					return;

				Json::Value root;
				Json::CharReaderBuilder builder;
				std::string errors;
				if (!Json::parseFromStream(builder, file, &root, &errors))
				{
					DIA_LOG_WARNING("Editor", "DiaAssetRuntimeEditor SessionContext: failed to parse context JSON");
					return;
				}

				if (root.isMember("pollInterval") && root["pollInterval"].isNumeric())
					mPollInterval = root["pollInterval"].asFloat();

				if (root.isMember("maxLogEntries") && root["maxLogEntries"].isNumeric())
					mMaxLogEntries = root["maxLogEntries"].asUInt();

				if (mPollInterval < 0.1f) mPollInterval = 0.1f;
				if (mMaxLogEntries < 10) mMaxLogEntries = 10;
				if (mMaxLogEntries > 4096) mMaxLogEntries = 4096;
			}

			void SessionContext::Save(const char* outputDir) const
			{
				std::string dirStr = outputDir;
				std::string cmd = "mkdir \"" + dirStr + "\" 2>nul";
				std::system(cmd.c_str());

				std::string path = std::string(outputDir) + "/" + kContextFileName;

				Json::Value root;
				root["pollInterval"] = mPollInterval;
				root["maxLogEntries"] = mMaxLogEntries;

				Json::StreamWriterBuilder writer;
				writer["indentation"] = "  ";
				std::ofstream file(path);
				if (!file.is_open())
				{
					DIA_LOG_WARNING("Editor", "DiaAssetRuntimeEditor SessionContext: could not write context file");
					return;
				}
				file << Json::writeString(writer, root);
			}

			void SessionContext::SetPollInterval(float interval)
			{
				if (interval < 0.1f) interval = 0.1f;
				mPollInterval = interval;
			}

			void SessionContext::SetMaxLogEntries(unsigned int max)
			{
				if (max < 10) max = 10;
				if (max > 4096) max = 4096;
				mMaxLogEntries = max;
			}
		}
	}
}
