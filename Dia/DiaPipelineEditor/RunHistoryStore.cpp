#include "DiaPipelineEditor/RunHistoryStore.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Dia::PipelineEditor;

static void CreateDirectoryRecursive(const char* path)
{
	char buf[512];
	strncpy_s(buf, sizeof(buf), path, _TRUNCATE);
	for (char* p = buf + 1; *p; ++p)
	{
		if (*p == '/' || *p == '\\')
		{
			char saved = *p;
			*p = '\0';
			CreateDirectoryA(buf, nullptr);
			*p = saved;
		}
	}
	CreateDirectoryA(buf, nullptr);
}

static void CopyFileToPath(const char* src, const char* dst)
{
	FILE* fIn = nullptr;
	fopen_s(&fIn, src, "rb");
	if (!fIn)
		return;

	FILE* fOut = nullptr;
	fopen_s(&fOut, dst, "wb");
	if (!fOut)
	{
		fclose(fIn);
		return;
	}

	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), fIn)) > 0)
		fwrite(buf, 1, n, fOut);

	fclose(fIn);
	fclose(fOut);
}

RunHistoryStore::RunHistoryStore()
	: mRunCount(0)
{
	mPluginRoot[0] = '\0';
	mHistoryDir[0] = '\0';
	mHistoryFilePath[0] = '\0';
	mContextFilePath[0] = '\0';
	mSessionsDir[0] = '\0';
	mSessionId[0] = '\0';
}

RunHistoryStore::~RunHistoryStore()
{
}

void RunHistoryStore::Initialize(const char* pluginRootPath, const char* sessionId)
{
	strncpy_s(mPluginRoot, sizeof(mPluginRoot), pluginRootPath, _TRUNCATE);
	strncpy_s(mSessionId, sizeof(mSessionId), sessionId, _TRUNCATE);
	snprintf(mHistoryDir, sizeof(mHistoryDir), "%s/pipeline-history", pluginRootPath);
	snprintf(mHistoryFilePath, sizeof(mHistoryFilePath), "%s/history.json", mHistoryDir);
	snprintf(mContextFilePath, sizeof(mContextFilePath), "%s/.context.json", pluginRootPath);
	snprintf(mSessionsDir, sizeof(mSessionsDir), "%s/.sessions", pluginRootPath);
	mRunCount = 0;

	CreateDirectoryRecursive(pluginRootPath);
	ArchiveStaleSession();
	WriteContext();
	EnsureDirectoryExists();
	LoadFromDisk();
	PruneSessions();
}

void RunHistoryStore::Shutdown()
{
	SaveToDisk();
}

void RunHistoryStore::RecordRun(const RunSummary& summary)
{
	if (mRunCount >= kMaxRuns)
	{
		for (int i = kMaxRuns - 1; i > 0; --i)
			mRuns[i] = mRuns[i - 1];
	}
	else
	{
		for (int i = mRunCount; i > 0; --i)
			mRuns[i] = mRuns[i - 1];
		++mRunCount;
	}

	mRuns[0] = summary;
	SaveToDisk();
}

int RunHistoryStore::GetCount() const
{
	return mRunCount;
}

const RunSummary& RunHistoryStore::GetRun(int index) const
{
	return mRuns[index];
}

void RunHistoryStore::LoadFromDisk()
{
	mRunCount = 0;

	FILE* f = nullptr;
	fopen_s(&f, mHistoryFilePath, "rb");
	if (!f)
		return;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0 || size > 1024 * 1024)
	{
		fclose(f);
		return;
	}

	char* buffer = new char[size + 1];
	fread(buffer, 1, size, f);
	buffer[size] = '\0';
	fclose(f);

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(buffer, buffer + size, root))
	{
		DIA_LOG_WARNING("PipelineEditor", "RunHistoryStore: corrupt history.json, starting fresh");
		delete[] buffer;
		return;
	}
	delete[] buffer;

	if (!root.isMember("runs") || !root["runs"].isArray())
	{
		DIA_LOG_WARNING("PipelineEditor", "RunHistoryStore: invalid history.json schema, starting fresh");
		return;
	}

	const Json::Value& runs = root["runs"];
	int count = static_cast<int>(runs.size());
	if (count > kMaxRuns)
		count = kMaxRuns;

	for (int i = 0; i < count; ++i)
	{
		mRuns[i] = DeserializeRun(runs[i]);
	}
	mRunCount = count;
}

void RunHistoryStore::SaveToDisk()
{
	EnsureDirectoryExists();

	Json::Value root;
	root["version"] = 1;
	root["runs"] = Json::Value(Json::arrayValue);

	for (int i = 0; i < mRunCount; ++i)
	{
		root["runs"].append(SerializeRun(mRuns[i]));
	}

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "  ";
	std::string json = Json::writeString(builder, root);

	char tmpPath[512];
	snprintf(tmpPath, sizeof(tmpPath), "%s/.history.json.tmp", mHistoryDir);

	FILE* f = nullptr;
	fopen_s(&f, tmpPath, "wb");
	if (!f)
	{
		DIA_LOG_WARNING("PipelineEditor", "RunHistoryStore: failed to write temp history file");
		return;
	}

	fwrite(json.c_str(), 1, json.size(), f);
	fclose(f);

	DeleteFileA(mHistoryFilePath);
	if (!MoveFileA(tmpPath, mHistoryFilePath))
	{
		DIA_LOG_WARNING("PipelineEditor", "RunHistoryStore: failed to rename temp history file");
	}
}

Json::Value RunHistoryStore::ToJson() const
{
	Json::Value arr(Json::arrayValue);
	for (int i = 0; i < mRunCount; ++i)
	{
		arr.append(SerializeRun(mRuns[i]));
	}
	return arr;
}

void RunHistoryStore::ArchiveStaleSession()
{
	FILE* f = nullptr;
	fopen_s(&f, mContextFilePath, "rb");
	if (!f)
		return;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0 || size > 64 * 1024)
	{
		fclose(f);
		return;
	}

	char* buffer = new char[size + 1];
	fread(buffer, 1, size, f);
	buffer[size] = '\0';
	fclose(f);

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(buffer, buffer + size, root))
	{
		delete[] buffer;
		return;
	}
	delete[] buffer;

	if (!root.isMember("sessionId") || !root["sessionId"].isString())
		return;

	const char* oldSessionId = root["sessionId"].asCString();
	if (strcmp(oldSessionId, mSessionId) == 0)
		return;

	CreateDirectoryA(mSessionsDir, nullptr);

	char archiveDir[512];
	snprintf(archiveDir, sizeof(archiveDir), "%s/%s", mSessionsDir, oldSessionId);
	CreateDirectoryA(archiveDir, nullptr);

	char archiveHistoryDir[512];
	snprintf(archiveHistoryDir, sizeof(archiveHistoryDir), "%s/pipeline-history", archiveDir);
	CreateDirectoryA(archiveHistoryDir, nullptr);

	// Move .context.json
	char archiveContext[512];
	snprintf(archiveContext, sizeof(archiveContext), "%s/.context.json", archiveDir);
	CopyFileToPath(mContextFilePath, archiveContext);

	// Move history.json
	char archiveHistory[512];
	snprintf(archiveHistory, sizeof(archiveHistory), "%s/history.json", archiveHistoryDir);
	if (GetFileAttributesA(mHistoryFilePath) != INVALID_FILE_ATTRIBUTES)
	{
		CopyFileToPath(mHistoryFilePath, archiveHistory);
		DeleteFileA(mHistoryFilePath);
	}

	DeleteFileA(mContextFilePath);
}

void RunHistoryStore::WriteContext()
{
	CreateDirectoryRecursive(mPluginRoot);

	Json::Value root;
	root["sessionId"] = mSessionId;

	SYSTEMTIME st;
	GetSystemTime(&st);
	char timeBuf[64];
	snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	root["started"] = timeBuf;

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "  ";
	std::string json = Json::writeString(builder, root);

	FILE* f = nullptr;
	fopen_s(&f, mContextFilePath, "wb");
	if (f)
	{
		fwrite(json.c_str(), 1, json.size(), f);
		fclose(f);
	}
}

void RunHistoryStore::PruneSessions()
{
	char searchPath[512];
	snprintf(searchPath, sizeof(searchPath), "%s/*", mSessionsDir);

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	struct SessionEntry
	{
		char name[256];
		FILETIME createTime;
	};

	SessionEntry entries[64];
	int entryCount = 0;

	do
	{
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;
		if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
			continue;
		if (entryCount < 64)
		{
			strncpy_s(entries[entryCount].name, sizeof(entries[entryCount].name), findData.cFileName, _TRUNCATE);
			entries[entryCount].createTime = findData.ftCreationTime;
			++entryCount;
		}
	} while (FindNextFileA(hFind, &findData));
	FindClose(hFind);

	if (entryCount <= kMaxArchivedSessions)
		return;

	// Sort by creation time (oldest first) using simple bubble sort
	for (int i = 0; i < entryCount - 1; ++i)
	{
		for (int j = 0; j < entryCount - i - 1; ++j)
		{
			ULARGE_INTEGER a, b;
			a.LowPart = entries[j].createTime.dwLowDateTime;
			a.HighPart = entries[j].createTime.dwHighDateTime;
			b.LowPart = entries[j + 1].createTime.dwLowDateTime;
			b.HighPart = entries[j + 1].createTime.dwHighDateTime;
			if (a.QuadPart > b.QuadPart)
			{
				SessionEntry tmp = entries[j];
				entries[j] = entries[j + 1];
				entries[j + 1] = tmp;
			}
		}
	}

	int toRemove = entryCount - kMaxArchivedSessions;
	for (int i = 0; i < toRemove; ++i)
	{
		char sessionDir[512];
		snprintf(sessionDir, sizeof(sessionDir), "%s/%s", mSessionsDir, entries[i].name);

		// Delete known files inside the archived session
		char path[512];
		snprintf(path, sizeof(path), "%s/.context.json", sessionDir);
		DeleteFileA(path);
		snprintf(path, sizeof(path), "%s/pipeline-history/history.json", sessionDir);
		DeleteFileA(path);
		snprintf(path, sizeof(path), "%s/pipeline-history", sessionDir);
		RemoveDirectoryA(path);
		RemoveDirectoryA(sessionDir);
	}
}

void RunHistoryStore::EnsureDirectoryExists()
{
	CreateDirectoryRecursive(mHistoryDir);
}

Json::Value RunHistoryStore::SerializeRun(const RunSummary& run)
{
	Json::Value val;
	val["target"] = run.target.AsChar();
	val["config"] = run.config.AsChar();
	val["passCount"] = run.passCount;
	val["failCount"] = run.failCount;
	val["totalDurationMs"] = run.totalDurationMs;
	val["startTimestamp"] = static_cast<double>(run.startTimestamp);
	val["interrupted"] = run.interrupted;
	return val;
}

RunSummary RunHistoryStore::DeserializeRun(const Json::Value& val)
{
	RunSummary run;
	if (val.isMember("target") && val["target"].isString())
		run.target = val["target"].asCString();
	if (val.isMember("config") && val["config"].isString())
		run.config = val["config"].asCString();
	if (val.isMember("passCount") && val["passCount"].isInt())
		run.passCount = val["passCount"].asInt();
	if (val.isMember("failCount") && val["failCount"].isInt())
		run.failCount = val["failCount"].asInt();
	if (val.isMember("totalDurationMs") && val["totalDurationMs"].isInt())
		run.totalDurationMs = val["totalDurationMs"].asInt();
	if (val.isMember("startTimestamp") && val["startTimestamp"].isDouble())
		run.startTimestamp = static_cast<float>(val["startTimestamp"].asDouble());
	if (val.isMember("interrupted") && val["interrupted"].isBool())
		run.interrupted = val["interrupted"].asBool();
	return run;
}
