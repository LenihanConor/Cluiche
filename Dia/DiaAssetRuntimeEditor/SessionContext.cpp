#include "DiaAssetRuntimeEditor/SessionContext.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <cstring>
#include <filesystem>
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
			{
				mStateFilter[0] = '\0';
				mIdSearchText[0] = '\0';
			}

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

				if (root.isMember("stateFilter") && root["stateFilter"].isString())
				{
					const char* sf = root["stateFilter"].asCString();
					strncpy(mStateFilter, sf, sizeof(mStateFilter) - 1);
					mStateFilter[sizeof(mStateFilter) - 1] = '\0';
				}

				if (root.isMember("idSearchText") && root["idSearchText"].isString())
				{
					const char* st = root["idSearchText"].asCString();
					strncpy(mIdSearchText, st, sizeof(mIdSearchText) - 1);
					mIdSearchText[sizeof(mIdSearchText) - 1] = '\0';
				}

				if (mPollInterval < 0.1f) mPollInterval = 0.1f;
				if (mMaxLogEntries < 10) mMaxLogEntries = 10;
				if (mMaxLogEntries > 4096) mMaxLogEntries = 4096;
			}

			void SessionContext::Save(const char* outputDir) const
			{
				std::filesystem::create_directories(outputDir);

				std::string path = std::string(outputDir) + "/" + kContextFileName;

				Json::Value root;
				root["pollInterval"] = mPollInterval;
				root["maxLogEntries"] = mMaxLogEntries;
				if (mStateFilter[0] != '\0')
					root["stateFilter"] = mStateFilter;
				if (mIdSearchText[0] != '\0')
					root["idSearchText"] = mIdSearchText;

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

			void SessionContext::SetStateFilter(const char* filter)
			{
				if (!filter)
				{
					mStateFilter[0] = '\0';
					return;
				}
				strncpy(mStateFilter, filter, sizeof(mStateFilter) - 1);
				mStateFilter[sizeof(mStateFilter) - 1] = '\0';
			}

			void SessionContext::SetIdSearchText(const char* text)
			{
				if (!text)
				{
					mIdSearchText[0] = '\0';
					return;
				}
				strncpy(mIdSearchText, text, sizeof(mIdSearchText) - 1);
				mIdSearchText[sizeof(mIdSearchText) - 1] = '\0';
			}
		}
	}
}
