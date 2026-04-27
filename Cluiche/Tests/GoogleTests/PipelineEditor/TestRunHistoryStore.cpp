// TestRunHistoryStore.cpp - Unit tests for RunHistoryStore

#include <gtest/gtest.h>
#include <DiaPipelineEditor/RunHistoryStore.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Dia::PipelineEditor;

class RunHistoryStoreTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		char tempDir[MAX_PATH];
		GetTempPathA(MAX_PATH, tempDir);
		GetTempFileNameA(tempDir, "rhs", 0, mTempDir);

		// GetTempFileName creates a file — delete it and create a directory instead
		DeleteFileA(mTempDir);
		CreateDirectoryA(mTempDir, nullptr);
	}

	void TearDown() override
	{
		CleanupDir(mTempDir);
	}

	void CleanupDir(const char* dir)
	{
		char searchPath[MAX_PATH];
		snprintf(searchPath, sizeof(searchPath), "%s\\*", dir);

		WIN32_FIND_DATAA fd;
		HANDLE hFind = FindFirstFileA(searchPath, &fd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
					continue;
				char fullPath[MAX_PATH];
				snprintf(fullPath, sizeof(fullPath), "%s\\%s", dir, fd.cFileName);
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					CleanupDir(fullPath);
				else
					DeleteFileA(fullPath);
			} while (FindNextFileA(hFind, &fd));
			FindClose(hFind);
		}
		RemoveDirectoryA(dir);
	}

	RunSummary MakeRun(const char* target, const char* config, int pass, int fail, int durationMs, float ts, bool interrupted = false)
	{
		RunSummary run;
		run.target = target;
		run.config = config;
		run.passCount = pass;
		run.failCount = fail;
		run.totalDurationMs = durationMs;
		run.startTimestamp = ts;
		run.interrupted = interrupted;
		return run;
	}

	char mTempDir[MAX_PATH];
};

// AC1: RecordRun stores a run summary
TEST_F(RunHistoryStoreTest, RecordRunStoresRun)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");

	RunSummary run = MakeRun("googletest", "Debug", 5, 0, 3000, 1000.0f);
	store.RecordRun(run);

	EXPECT_EQ(store.GetCount(), 1);
	EXPECT_STREQ(store.GetRun(0).target.AsChar(), "googletest");
	EXPECT_STREQ(store.GetRun(0).config.AsChar(), "Debug");
	EXPECT_EQ(store.GetRun(0).passCount, 5);
	EXPECT_EQ(store.GetRun(0).failCount, 0);
	EXPECT_EQ(store.GetRun(0).totalDurationMs, 3000);
}

// AC1: newest run is at index 0
TEST_F(RunHistoryStoreTest, NewestRunIsAtIndex0)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");

	store.RecordRun(MakeRun("first", "Debug", 1, 0, 1000, 100.0f));
	store.RecordRun(MakeRun("second", "Release", 2, 0, 2000, 200.0f));

	EXPECT_EQ(store.GetCount(), 2);
	EXPECT_STREQ(store.GetRun(0).target.AsChar(), "second");
	EXPECT_STREQ(store.GetRun(1).target.AsChar(), "first");
}

// AC2: cap at 10 entries, oldest evicted
TEST_F(RunHistoryStoreTest, CapAt10Entries)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");

	for (int i = 0; i < 11; ++i)
	{
		char name[32];
		snprintf(name, sizeof(name), "run%d", i);
		store.RecordRun(MakeRun(name, "Debug", i, 0, i * 100, static_cast<float>(i)));
	}

	EXPECT_EQ(store.GetCount(), 10);
	EXPECT_STREQ(store.GetRun(0).target.AsChar(), "run10");
	EXPECT_STREQ(store.GetRun(9).target.AsChar(), "run1");
}

// AC3: SaveToDisk writes to expected path
TEST_F(RunHistoryStoreTest, SaveToDiskCreatesFile)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");
	store.RecordRun(MakeRun("googletest", "Debug", 3, 1, 5000, 1000.0f));

	char historyFile[MAX_PATH];
	snprintf(historyFile, sizeof(historyFile), "%s\\pipeline-history\\history.json", mTempDir);

	DWORD attrs = GetFileAttributesA(historyFile);
	EXPECT_NE(attrs, INVALID_FILE_ATTRIBUTES);
}

// AC4: LoadFromDisk restores runs across instances
TEST_F(RunHistoryStoreTest, LoadFromDiskRestoresRuns)
{
	{
		RunHistoryStore store;
		store.Initialize(mTempDir, "s-test-session");
		store.RecordRun(MakeRun("googletest", "Debug", 3, 1, 5000, 1000.0f));
		store.RecordRun(MakeRun("cluichetest", "Release", 2, 0, 3000, 2000.0f));
		store.Shutdown();
	}

	{
		RunHistoryStore store2;
		store2.Initialize(mTempDir, "s-test-session");
		EXPECT_EQ(store2.GetCount(), 2);
		EXPECT_STREQ(store2.GetRun(0).target.AsChar(), "cluichetest");
		EXPECT_STREQ(store2.GetRun(1).target.AsChar(), "googletest");
		EXPECT_EQ(store2.GetRun(0).passCount, 2);
		EXPECT_EQ(store2.GetRun(1).failCount, 1);
	}
}

// AC9: corrupt JSON starts fresh
TEST_F(RunHistoryStoreTest, CorruptJsonStartsFresh)
{
	char historyDir[MAX_PATH];
	snprintf(historyDir, sizeof(historyDir), "%s\\pipeline-history", mTempDir);
	CreateDirectoryA(historyDir, nullptr);

	char historyFile[MAX_PATH];
	snprintf(historyFile, sizeof(historyFile), "%s\\history.json", historyDir);

	FILE* f = nullptr;
	fopen_s(&f, historyFile, "w");
	if (f) { fputs("not json at all {{[", f); fclose(f); }

	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");
	EXPECT_EQ(store.GetCount(), 0);
}

// AC10: Initialize creates pipeline-history directory if missing
TEST_F(RunHistoryStoreTest, InitializeCreatesHistoryDirectory)
{
	// pluginRootPath exists (mTempDir), but pipeline-history/ does not yet
	char historyDir[MAX_PATH];
	snprintf(historyDir, sizeof(historyDir), "%s\\pipeline-history", mTempDir);
	DWORD attrsBefore = GetFileAttributesA(historyDir);

	// Remove pipeline-history if it was created in SetUp
	if (attrsBefore != INVALID_FILE_ATTRIBUTES)
		RemoveDirectoryA(historyDir);

	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");
	store.RecordRun(MakeRun("test", "Debug", 1, 0, 100, 1.0f));

	char historyFile[MAX_PATH];
	snprintf(historyFile, sizeof(historyFile), "%s\\pipeline-history\\history.json", mTempDir);

	DWORD attrs = GetFileAttributesA(historyFile);
	EXPECT_NE(attrs, INVALID_FILE_ATTRIBUTES);
}

// AC8: ToJson returns correct format
TEST_F(RunHistoryStoreTest, ToJsonReturnsCorrectFormat)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");
	store.RecordRun(MakeRun("googletest", "Debug", 3, 1, 5000, 1000.0f, false));
	store.RecordRun(MakeRun("cluichetest", "Release", 2, 0, 3000, 2000.0f, true));

	Json::Value arr = store.ToJson();
	ASSERT_EQ(arr.size(), 2u);
	EXPECT_STREQ(arr[0u]["target"].asCString(), "cluichetest");
	EXPECT_TRUE(arr[0u]["interrupted"].asBool());
	EXPECT_STREQ(arr[1u]["target"].asCString(), "googletest");
	EXPECT_FALSE(arr[1u]["interrupted"].asBool());
}

// Interrupted run recorded correctly
TEST_F(RunHistoryStoreTest, RecordsInterruptedRun)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");
	store.RecordRun(MakeRun("test", "Debug", 1, 0, 500, 100.0f, true));

	EXPECT_TRUE(store.GetRun(0).interrupted);
}

// Empty store returns count 0
TEST_F(RunHistoryStoreTest, EmptyStoreReturnsCount0)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-test-session");
	EXPECT_EQ(store.GetCount(), 0);
}

// SED-021: Initialize writes .context.json
TEST_F(RunHistoryStoreTest, WritesContextJson)
{
	RunHistoryStore store;
	store.Initialize(mTempDir, "s-20260427-1030");

	char contextPath[MAX_PATH];
	snprintf(contextPath, sizeof(contextPath), "%s\\.context.json", mTempDir);

	DWORD attrs = GetFileAttributesA(contextPath);
	EXPECT_NE(attrs, INVALID_FILE_ATTRIBUTES);

	// Verify it contains the session ID
	FILE* f = nullptr;
	fopen_s(&f, contextPath, "rb");
	ASSERT_NE(f, nullptr);
	char buf[1024];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	buf[n] = '\0';
	fclose(f);

	EXPECT_NE(strstr(buf, "s-20260427-1030"), nullptr);
}

// SED-021: new session archives old session data
TEST_F(RunHistoryStoreTest, ArchivesStaleSession)
{
	// Session 1: create some history
	{
		RunHistoryStore store;
		store.Initialize(mTempDir, "s-session-old");
		store.RecordRun(MakeRun("googletest", "Debug", 5, 0, 3000, 1000.0f));
		store.Shutdown();
	}

	// Session 2: different ID — should archive session 1
	{
		RunHistoryStore store;
		store.Initialize(mTempDir, "s-session-new");
		// Old history should be archived, new store starts fresh
		EXPECT_EQ(store.GetCount(), 0);
	}

	// Verify archive exists
	char archivedHistory[MAX_PATH];
	snprintf(archivedHistory, sizeof(archivedHistory),
		"%s\\.sessions\\s-session-old\\pipeline-history\\history.json", mTempDir);
	DWORD attrs = GetFileAttributesA(archivedHistory);
	EXPECT_NE(attrs, INVALID_FILE_ATTRIBUTES);

	char archivedContext[MAX_PATH];
	snprintf(archivedContext, sizeof(archivedContext),
		"%s\\.sessions\\s-session-old\\.context.json", mTempDir);
	attrs = GetFileAttributesA(archivedContext);
	EXPECT_NE(attrs, INVALID_FILE_ATTRIBUTES);
}

// SED-021: same session ID does not archive
TEST_F(RunHistoryStoreTest, SameSessionDoesNotArchive)
{
	{
		RunHistoryStore store;
		store.Initialize(mTempDir, "s-same");
		store.RecordRun(MakeRun("googletest", "Debug", 3, 0, 1000, 100.0f));
		store.Shutdown();
	}

	{
		RunHistoryStore store;
		store.Initialize(mTempDir, "s-same");
		// Same session — history should persist, not be archived
		EXPECT_EQ(store.GetCount(), 1);
		EXPECT_STREQ(store.GetRun(0).target.AsChar(), "googletest");
	}
}
