#include "DiaAssetCatalogueEditor/SessionContext.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <fstream>
#include <string>

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

				std::string path = std::string(outputDir) + "/" + kContextFileName;
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
					const std::string& val = root["lastManifestPath"].asString();
					strncpy_s(mLastManifestPath, kMaxPathLength, val.c_str(), _TRUNCATE);
				}
			}

			void SessionContext::Save(const char* outputDir) const
			{
				// Ensure output directory exists
				std::string dirStr = outputDir;
				// Create directories (best-effort; CreateDirectoryA is no-op if exists)
				std::string cmd = "mkdir \"" + dirStr + "\" 2>nul";
				std::system(cmd.c_str());

				std::string path = std::string(outputDir) + "/" + kContextFileName;

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
