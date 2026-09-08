#include "DiaSchemaBrowser/DiaSchemaBrowserPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Dia::SchemaBrowser;

REGISTER_EDITOR_PLUGIN(DiaSchemaBrowserPlugin, "DiaSchemaBrowser")

namespace
{
	const char* kSchemaExtension = ".diagamemessages";

	// Directories that never contain hand-authored schema declarations and are
	// expensive to walk. Pruned during discovery.
	const char* kPrunedDirs[] = {
		".git", ".vs", ".venv", "node_modules", "out", "bin", "obj",
		"External", "dist", "packages", "__pycache__", ".claude"
	};

	bool IsPrunedDir(const char* name)
	{
		for (const char* pruned : kPrunedDirs)
		{
			if (_stricmp(name, pruned) == 0)
				return true;
		}
		return false;
	}

	bool HasSchemaExtension(const char* name)
	{
		size_t nameLen = strlen(name);
		size_t extLen  = strlen(kSchemaExtension);
		if (nameLen < extLen)
			return false;
		return _stricmp(name + (nameLen - extLen), kSchemaExtension) == 0;
	}

	// Convert an absolute path into a repo-relative path with forward slashes.
	std::string MakeRelative(const std::string& repoRoot, const std::string& fullPath)
	{
		std::string rel = fullPath;
		if (rel.size() > repoRoot.size() &&
			_strnicmp(rel.c_str(), repoRoot.c_str(), repoRoot.size()) == 0)
		{
			rel = rel.substr(repoRoot.size());
		}
		while (!rel.empty() && (rel.front() == '\\' || rel.front() == '/'))
			rel.erase(rel.begin());
		for (char& c : rel)
			if (c == '\\') c = '/';
		return rel;
	}
}

namespace Dia
{
	namespace SchemaBrowser
	{
		static const Dia::Core::StringCRC kCmdScan("schema.scan");

		void DiaSchemaBrowserPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
		{
			DIA_LOG_INFO("Editor", "DiaSchemaBrowserPlugin: OnLoad");
			mBridge = context.mBridge;
			mRepoRoot[0] = '\0';
			ResolveRepoRoot();
			RegisterRequestHandlers();
		}

		void DiaSchemaBrowserPlugin::OnUnload()
		{
			DIA_LOG_INFO("Editor", "DiaSchemaBrowserPlugin: OnUnload");
			mBridge = nullptr;
		}

		void DiaSchemaBrowserPlugin::OnUpdate(float /*deltaTime*/)
		{
		}

		void DiaSchemaBrowserPlugin::ResolveRepoRoot()
		{
			char exePath[1024];
			GetModuleFileNameA(NULL, exePath, sizeof(exePath));
			char* sep = strrchr(exePath, '\\');
			if (sep) *sep = '\0';
			strncpy_s(mRepoRoot, sizeof(mRepoRoot), exePath, _TRUNCATE);

			for (int i = 0; i < 10; ++i)
			{
				char probe[1024 + 16];
				snprintf(probe, sizeof(probe), "%s\\pipeline.toml", mRepoRoot);
				if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES)
					return;
				char* up = strrchr(mRepoRoot, '\\');
				if (up == nullptr)
					break;
				*up = '\0';
			}
			DIA_LOG_WARNING("Editor", "DiaSchemaBrowserPlugin: could not locate repo root (pipeline.toml)");
		}

		void DiaSchemaBrowserPlugin::CollectSchemaFiles(const char* dir, Json::Value& outFiles) const
		{
			char searchPath[2048];
			snprintf(searchPath, sizeof(searchPath), "%s\\*", dir);

			WIN32_FIND_DATAA fd;
			HANDLE h = FindFirstFileA(searchPath, &fd);
			if (h == INVALID_HANDLE_VALUE)
				return;

			do
			{
				if (fd.cFileName[0] == '.' &&
					(fd.cFileName[1] == '\0' || (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0')))
					continue; // "." / ".."

				char child[2048];
				snprintf(child, sizeof(child), "%s\\%s", dir, fd.cFileName);

				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (!IsPrunedDir(fd.cFileName))
						CollectSchemaFiles(child, outFiles);
				}
				else if (HasSchemaExtension(fd.cFileName))
				{
					outFiles.append(std::string(child));
				}
			} while (FindNextFileA(h, &fd));

			FindClose(h);
		}

		bool DiaSchemaBrowserPlugin::ParseSchemaFile(const char* fullPath, Json::Value& outDoc) const
		{
			// Read-only: open strictly for reading.
			std::ifstream in(fullPath, std::ios::in | std::ios::binary);
			if (!in.is_open())
				return false;

			std::stringstream buffer;
			buffer << in.rdbuf();
			const std::string text = buffer.str();

			Json::CharReaderBuilder builder;
			std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
			std::string errs;
			const char* begin = text.c_str();
			const char* end   = begin + text.size();
			if (!reader->parse(begin, end, &outDoc, &errs))
			{
				DIA_LOG_WARNING("Editor", "DiaSchemaBrowserPlugin: failed to parse %s: %s", fullPath, errs.c_str());
				return false;
			}
			return true;
		}

		Json::Value DiaSchemaBrowserPlugin::BuildScanResult() const
		{
			Json::Value result;
			Json::Value documents(Json::arrayValue);

			Json::Value files(Json::arrayValue);
			if (mRepoRoot[0] != '\0')
				CollectSchemaFiles(mRepoRoot, files);

			const std::string repoRoot(mRepoRoot);
			int messageCount = 0;

			for (const Json::Value& fileVal : files)
			{
				const std::string full = fileVal.asString();

				Json::Value doc;
				if (!ParseSchemaFile(full.c_str(), doc))
					continue;

				// Only surface valid diagamemessages documents.
				if (!doc.isObject() || !doc.isMember("messages") || !doc["messages"].isArray())
					continue;

				Json::Value entry(Json::objectValue);
				entry["file"] = MakeRelative(repoRoot, full);
				entry["doc"]  = doc; // raw parsed document; TS interprets semantics
				documents.append(entry);

				messageCount += static_cast<int>(doc["messages"].size());
			}

			result["documents"]    = documents;
			result["fileCount"]    = static_cast<int>(documents.size());
			result["messageCount"] = messageCount;
			return result;
		}

		void DiaSchemaBrowserPlugin::RegisterRequestHandlers()
		{
			if (!mBridge)
				return;

			mBridge->RegisterRequestHandler(kCmdScan,
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result = BuildScanResult();
					DIA_LOG_INFO("Editor",
						"DiaSchemaBrowserPlugin: scan found %d file(s), %d message(s)",
						result["fileCount"].asInt(), result["messageCount"].asInt());
					return result;
				});
		}
	}
}
