#include "DiaAssetCatalogueEditor/SessionContext.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <cstring>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			static const char* kContextFileName = "DiaAssetCatalogueEditor.context.json";

			SessionContext::SessionContext()
			{
				mLastManifestPath[0] = '\0';
			}

			void SessionContext::Load(const char* outputDir)
			{
				mLastManifestPath[0] = '\0';

				char path[kMaxPathLength];
				_snprintf_s(path, kMaxPathLength, _TRUNCATE, "%s/%s", outputDir, kContextFileName);

				std::ifstream file(path);
				if (!file.is_open())
					return;

				Json::Value root;
				Json::CharReaderBuilder builder;
				std::string errors;
				if (!Json::parseFromStream(builder, file, &root, &errors))
				{
					DIA_LOG_WARNING("Editor", "SessionContext: failed to parse context JSON");
					return;
				}

				if (root.isMember("lastManifestPath") && root["lastManifestPath"].isString())
				{
					strncpy_s(mLastManifestPath, kMaxPathLength,
						root["lastManifestPath"].asCString(), _TRUNCATE);
				}
			}

			void SessionContext::Save(const char* outputDir) const
			{
				CreateDirectoryA(outputDir, nullptr);

				char path[kMaxPathLength];
				_snprintf_s(path, kMaxPathLength, _TRUNCATE, "%s/%s", outputDir, kContextFileName);

				Json::Value root;
				root["lastManifestPath"] = mLastManifestPath;

				Json::StreamWriterBuilder writer;
				writer["indentation"] = "  ";
				std::ofstream file(path);
				if (!file.is_open())
				{
					DIA_LOG_WARNING("Editor", "SessionContext: could not write context file");
					return;
				}
				file << Json::writeString(writer, root);
			}

			const char* SessionContext::GetLastManifestPath() const
			{
				return mLastManifestPath;
			}

			void SessionContext::SetLastManifestPath(const char* path)
			{
				strncpy_s(mLastManifestPath, kMaxPathLength, path, _TRUNCATE);
			}

			void SessionContext::ClearLastManifestPath()
			{
				mLastManifestPath[0] = '\0';
			}

			bool SessionContext::HasLastManifestPath() const
			{
				return mLastManifestPath[0] != '\0';
			}
		}
	}
}
