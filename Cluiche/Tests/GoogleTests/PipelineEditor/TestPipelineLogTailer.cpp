#include <gtest/gtest.h>
#include <DiaPipelineEditor/PipelineLogTailer.h>
#include <DiaPipelineEditor/PipelineEvent.h>
#include <DiaCore/Architecture/Observer.h>

#include <cstdio>
#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Dia::PipelineEditor;

namespace
{
	class TempFile
	{
	public:
		TempFile()
		{
			char tmpDir[MAX_PATH];
			GetTempPathA(MAX_PATH, tmpDir);
			GetTempFileNameA(tmpDir, "ndjson", 0, mPath);
			DeleteFileA(mPath);
		}

		~TempFile()
		{
			DeleteFileA(mPath);
		}

		const char* Path() const { return mPath; }

		void Write(const char* content) const
		{
			FILE* f = nullptr;
			fopen_s(&f, mPath, "ab");
			if (f)
			{
				fwrite(content, 1, strlen(content), f);
				fclose(f);
			}
		}

		void Overwrite(const char* content) const
		{
			FILE* f = nullptr;
			fopen_s(&f, mPath, "wb");
			if (f)
			{
				fwrite(content, 1, strlen(content), f);
				fclose(f);
			}
		}

		void Remove() const
		{
			DeleteFileA(mPath);
		}

	private:
		char mPath[MAX_PATH];
	};

	class MockObserver : public Dia::Core::Observer
	{
	public:
		int notifyCount = 0;
		int lastMessage = 0;

		void ObserverNotification(const Dia::Core::ObserverSubject* /*subject*/, int message) override
		{
			notifyCount++;
			lastMessage = message;
		}
	};

	const char* kRunStarted =
		"{\"event\":\"OnRunStarted\",\"schema\":\"dia.output.v1\",\"system\":\"pipeline\","
		"\"target\":\"googletest\",\"config\":\"Debug\","
		"\"stages\":[\"compile-code\",\"package\"],\"ts\":1714123456.0}\n";

	const char* kStageStarted =
		"{\"event\":\"OnStageStarted\",\"system\":\"pipeline\",\"stage\":\"compile-code\",\"ts\":1714123456.1}\n";

	const char* kLogLine =
		"{\"event\":\"OnLogLine\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"level\":\"info\",\"message\":\"Building GoogleTests.vcxproj\",\"ts\":1714123456.2}\n";

	const char* kStageCompleted =
		"{\"event\":\"OnStageCompleted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"durationMs\":4200,\"ts\":1714123460.3}\n";

	const char* kStageFailed =
		"{\"event\":\"OnStageFailed\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"error\":\"msbuild exited 1\",\"durationMs\":4200,\"ts\":1714123460.3}\n";

	const char* kRunCompleted =
		"{\"event\":\"OnRunCompleted\",\"system\":\"pipeline\",\"durationMs\":5100,\"ts\":1714123461.1}\n";

	const char* kRunFailed =
		"{\"event\":\"OnRunFailed\",\"system\":\"pipeline\",\"durationMs\":5100,"
		"\"error\":\"1 stage failed\",\"ts\":1714123461.1}\n";
}

// AC1: Polls file, reads only new bytes since last seek position
TEST(PipelineLogTailer, AC1_IncrementalReads)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 1);

	tmp.Write(kStageStarted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 2);

	tmp.Write(kStageCompleted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 3);

	tailer.Shutdown();
}

// AC1: No new data means no new events
TEST(PipelineLogTailer, AC1_NoNewDataNoNewEvents)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 1);

	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 1);

	tailer.Shutdown();
}

// AC2: Parses all PipelineEvent fields correctly
TEST(PipelineLogTailer, AC2_ParsesAllFields)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(kLogLine);
	tmp.Write(kStageCompleted);
	tailer.Poll();

	ASSERT_EQ(tailer.GetEventCount(), 4);

	const PipelineEvent& runEvt = tailer.GetEvent(0);
	EXPECT_EQ(runEvt.eventType, Dia::Core::StringCRC("OnRunStarted"));
	EXPECT_EQ(runEvt.system, Dia::Core::StringCRC("pipeline"));
	EXPECT_NEAR(runEvt.timestampSec, 1714123456.0f, 1.0f);

	const PipelineEvent& stageEvt = tailer.GetEvent(1);
	EXPECT_EQ(stageEvt.eventType, Dia::Core::StringCRC("OnStageStarted"));
	EXPECT_EQ(stageEvt.stage, Dia::Core::StringCRC("compile-code"));

	const PipelineEvent& logEvt = tailer.GetEvent(2);
	EXPECT_EQ(logEvt.eventType, Dia::Core::StringCRC("OnLogLine"));
	ASSERT_NE(logEvt.level, nullptr);
	EXPECT_STREQ(logEvt.level, "info");
	ASSERT_NE(logEvt.detail, nullptr);
	EXPECT_STREQ(logEvt.detail, "Building GoogleTests.vcxproj");

	const PipelineEvent& compEvt = tailer.GetEvent(3);
	EXPECT_EQ(compEvt.eventType, Dia::Core::StringCRC("OnStageCompleted"));
	EXPECT_EQ(compEvt.durationMs, 4200);

	tailer.Shutdown();
}

// AC3: Detects file truncation and resets state
TEST(PipelineLogTailer, AC3_DetectsTruncation)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(kStageCompleted);
	tmp.Write(kRunCompleted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 4);

	tmp.Overwrite(kRunStarted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 1);
	EXPECT_TRUE(tailer.IsRunInProgress());

	tailer.Shutdown();
}

// AC4: Maintains RunSummary with pass/fail counts
TEST(PipelineLogTailer, AC4_RunSummaryPassFail)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(kStageCompleted);

	const char* stage2Started =
		"{\"event\":\"OnStageStarted\",\"system\":\"pipeline\",\"stage\":\"package\",\"ts\":1714123461.0}\n";
	const char* stage2Failed =
		"{\"event\":\"OnStageFailed\",\"system\":\"pipeline\",\"stage\":\"package\","
		"\"error\":\"packaging failed\",\"durationMs\":900,\"ts\":1714123461.9}\n";

	tmp.Write(stage2Started);
	tmp.Write(stage2Failed);
	tmp.Write(kRunFailed);
	tailer.Poll();

	const RunSummary& summary = tailer.GetCurrentRunSummary();
	EXPECT_EQ(summary.target, Dia::Core::StringCRC("googletest"));
	EXPECT_EQ(summary.config, Dia::Core::StringCRC("Debug"));
	EXPECT_EQ(summary.passCount, 1);
	EXPECT_EQ(summary.failCount, 1);
	EXPECT_EQ(summary.totalDurationMs, 5100);
	EXPECT_FALSE(summary.interrupted);
	EXPECT_FALSE(tailer.IsRunInProgress());

	tailer.Shutdown();
}

// AC5: Notifies observers when new events arrive
TEST(PipelineLogTailer, AC5_NotifiesObservers)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	MockObserver observer;

	tailer.Initialize(tmp.Path());
	tailer.RegisterObserver(&observer);

	tmp.Write(kRunStarted);
	tailer.Poll();
	EXPECT_GE(observer.notifyCount, 1);

	int prevCount = observer.notifyCount;
	tmp.Write(kStageStarted);
	tailer.Poll();
	EXPECT_GT(observer.notifyCount, prevCount);

	tailer.UnregisterObserver(&observer);

	prevCount = observer.notifyCount;
	tmp.Write(kStageCompleted);
	tailer.Poll();
	EXPECT_EQ(observer.notifyCount, prevCount);

	tailer.Shutdown();
}

// AC6: Detects interrupted runs (cannot easily test real-time timeout in unit test,
// but we verify the state when interrupted flag is set)
TEST(PipelineLogTailer, AC6_InterruptedState)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tailer.Poll();

	EXPECT_TRUE(tailer.IsRunInProgress());
	EXPECT_FALSE(tailer.GetCurrentRunSummary().interrupted);
	EXPECT_EQ(tailer.GetCurrentRunSummary().passCount, 0);
	EXPECT_EQ(tailer.GetCurrentRunSummary().failCount, 0);

	tailer.Shutdown();
}

// AC7: Handles missing file gracefully
TEST(PipelineLogTailer, AC7_MissingFileNoCrash)
{
	PipelineLogTailer tailer;
	tailer.Initialize("C:\\nonexistent\\path\\missing.ndjson");

	tailer.Poll();
	tailer.Poll();
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 0);
	EXPECT_FALSE(tailer.IsRunInProgress());

	tailer.Shutdown();
}

// AC7: File appears later
TEST(PipelineLogTailer, AC7_FileAppearsLater)
{
	TempFile tmp;
	tmp.Remove();

	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 0);

	tmp.Write(kRunStarted);
	tailer.Poll();
	EXPECT_EQ(tailer.GetEventCount(), 1);
	EXPECT_TRUE(tailer.IsRunInProgress());

	tailer.Shutdown();
}

// AC8: Warns on unknown schema (verified by not crashing + still parsing events)
TEST(PipelineLogTailer, AC8_UnknownSchemaStillParses)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	const char* unknownSchema =
		"{\"event\":\"OnRunStarted\",\"schema\":\"dia.output.v2\",\"system\":\"pipeline\","
		"\"target\":\"googletest\",\"config\":\"Debug\","
		"\"stages\":[\"compile-code\"],\"ts\":1714123456.0}\n";

	tmp.Write(unknownSchema);
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 1);
	EXPECT_TRUE(tailer.IsRunInProgress());

	tailer.Shutdown();
}

// AC9: Handles malformed JSON lines without crashing
TEST(PipelineLogTailer, AC9_MalformedJsonSkipped)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tmp.Write("this is not valid json\n");
	tmp.Write("{broken json{{\n");
	tmp.Write(kStageStarted);
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 2);

	const PipelineEvent& evt0 = tailer.GetEvent(0);
	EXPECT_EQ(evt0.eventType, Dia::Core::StringCRC("OnRunStarted"));

	const PipelineEvent& evt1 = tailer.GetEvent(1);
	EXPECT_EQ(evt1.eventType, Dia::Core::StringCRC("OnStageStarted"));

	tailer.Shutdown();
}

// AC9: Malformed line between valid lines doesn't corrupt state
TEST(PipelineLogTailer, AC9_MalformedBetweenValidPreservesState)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write("not json\n");
	tmp.Write(kStageCompleted);
	tmp.Write(kRunCompleted);
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 4);
	EXPECT_FALSE(tailer.IsRunInProgress());
	EXPECT_EQ(tailer.GetCurrentRunSummary().passCount, 1);

	tailer.Shutdown();
}

// ---------------------------------------------------------------------------
// Step event tests
// ---------------------------------------------------------------------------

// Step events are appended to mEvents; mUnmatchedStartedCount is not touched
TEST(PipelineLogTailer, StepEvents_AppendedWithoutAffectingInterruptDetection)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	const char* stepStarted =
		"{\"event\":\"OnStepStarted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"msbuild\",\"ts\":1714123457.0}\n";
	const char* stepCompleted =
		"{\"event\":\"OnStepCompleted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"msbuild\",\"durationMs\":3000,\"ts\":1714123460.0}\n";

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(stepStarted);
	tmp.Write(stepCompleted);
	tmp.Write(kStageCompleted);
	tmp.Write(kRunCompleted);
	tailer.Poll();

	// All 6 events stored
	EXPECT_EQ(tailer.GetEventCount(), 6);

	// Step event fields parsed correctly
	const PipelineEvent& stepEvt = tailer.GetEvent(2);
	EXPECT_EQ(stepEvt.eventType, Dia::Core::StringCRC("OnStepStarted"));
	EXPECT_EQ(stepEvt.stage, Dia::Core::StringCRC("compile-code"));
	EXPECT_EQ(stepEvt.step, Dia::Core::StringCRC("msbuild"));

	const PipelineEvent& stepDoneEvt = tailer.GetEvent(3);
	EXPECT_EQ(stepDoneEvt.eventType, Dia::Core::StringCRC("OnStepCompleted"));
	EXPECT_EQ(stepDoneEvt.durationMs, 3000);

	// Run summary unaffected by step events
	const RunSummary& summary = tailer.GetCurrentRunSummary();
	EXPECT_EQ(summary.passCount, 1);
	EXPECT_EQ(summary.failCount, 0);
	EXPECT_FALSE(tailer.IsRunInProgress());

	tailer.Shutdown();
}

// Step events do NOT count toward mUnmatchedStartedCount — a stageStarted with only
// step events between it and its completed should not corrupt interrupt detection.
TEST(PipelineLogTailer, StepEvents_DoNotCorruptInterruptDetection)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	const char* stepStarted =
		"{\"event\":\"OnStepStarted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"protobuf\",\"ts\":1714123456.5}\n";
	const char* stepCompleted =
		"{\"event\":\"OnStepCompleted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"protobuf\",\"durationMs\":500,\"ts\":1714123457.0}\n";

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);     // mUnmatchedStartedCount = 1
	tmp.Write(stepStarted);       // must NOT increment mUnmatchedStartedCount
	tmp.Write(stepCompleted);     // must NOT decrement mUnmatchedStartedCount
	tmp.Write(kStageCompleted);   // mUnmatchedStartedCount = 0
	tmp.Write(kRunCompleted);
	tailer.Poll();

	EXPECT_FALSE(tailer.IsRunInProgress());
	EXPECT_EQ(tailer.GetCurrentRunSummary().passCount, 1);
	EXPECT_FALSE(tailer.GetCurrentRunSummary().interrupted);

	tailer.Shutdown();
}

// OnStepFailed is stored but does NOT affect stage pass/fail counts
TEST(PipelineLogTailer, StepEvents_StepFailedStoredButDoesNotIncrementFailCount)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	const char* stepStarted =
		"{\"event\":\"OnStepStarted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"msbuild\",\"ts\":1714123457.0}\n";
	const char* stepFailed =
		"{\"event\":\"OnStepFailed\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"msbuild\",\"error\":\"msbuild exited 1\",\"durationMs\":2000,\"ts\":1714123459.0}\n";

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(stepStarted);
	tmp.Write(stepFailed);
	tmp.Write(kStageFailed);  // Stage itself fails
	tmp.Write(kRunFailed);
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 6);

	const PipelineEvent& stepFailEvt = tailer.GetEvent(3);
	EXPECT_EQ(stepFailEvt.eventType, Dia::Core::StringCRC("OnStepFailed"));
	ASSERT_NE(stepFailEvt.error, nullptr);
	EXPECT_STREQ(stepFailEvt.error, "msbuild exited 1");
	EXPECT_EQ(stepFailEvt.durationMs, 2000);

	// Stage-level counts: step failure doesn't add to failCount — only stage failure does
	const RunSummary& summary = tailer.GetCurrentRunSummary();
	EXPECT_EQ(summary.passCount, 0);
	EXPECT_EQ(summary.failCount, 1);

	tailer.Shutdown();
}

// Multiple steps across multiple stages all stored correctly
TEST(PipelineLogTailer, StepEvents_MultipleStagesMultipleSteps)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	tailer.Initialize(tmp.Path());

	const char* stage2Started =
		"{\"event\":\"OnStageStarted\",\"system\":\"pipeline\",\"stage\":\"deploy\",\"ts\":1714123462.0}\n";
	const char* step1Started =
		"{\"event\":\"OnStepStarted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"msbuild\",\"ts\":1714123456.5}\n";
	const char* step1Completed =
		"{\"event\":\"OnStepCompleted\",\"system\":\"pipeline\",\"stage\":\"compile-code\","
		"\"step\":\"msbuild\",\"durationMs\":4000,\"ts\":1714123460.5}\n";
	const char* step2Started =
		"{\"event\":\"OnStepStarted\",\"system\":\"pipeline\",\"stage\":\"deploy\","
		"\"step\":\"copy-files\",\"ts\":1714123462.1}\n";
	const char* step2Completed =
		"{\"event\":\"OnStepCompleted\",\"system\":\"pipeline\",\"stage\":\"deploy\","
		"\"step\":\"copy-files\",\"durationMs\":200,\"ts\":1714123462.3}\n";
	const char* stage2Completed =
		"{\"event\":\"OnStageCompleted\",\"system\":\"pipeline\",\"stage\":\"deploy\","
		"\"durationMs\":400,\"ts\":1714123462.4}\n";

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(step1Started);
	tmp.Write(step1Completed);
	tmp.Write(kStageCompleted);
	tmp.Write(stage2Started);
	tmp.Write(step2Started);
	tmp.Write(step2Completed);
	tmp.Write(stage2Completed);
	tmp.Write(kRunCompleted);
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 10);
	EXPECT_EQ(tailer.GetCurrentRunSummary().passCount, 2);
	EXPECT_EQ(tailer.GetCurrentRunSummary().failCount, 0);
	EXPECT_FALSE(tailer.IsRunInProgress());

	// Verify step stages are tagged correctly
	const PipelineEvent& s1 = tailer.GetEvent(2);
	EXPECT_EQ(s1.stage, Dia::Core::StringCRC("compile-code"));
	EXPECT_EQ(s1.step, Dia::Core::StringCRC("msbuild"));

	const PipelineEvent& s2 = tailer.GetEvent(6);
	EXPECT_EQ(s2.stage, Dia::Core::StringCRC("deploy"));
	EXPECT_EQ(s2.step, Dia::Core::StringCRC("copy-files"));

	tailer.Shutdown();
}

// Full pipeline run end-to-end
TEST(PipelineLogTailer, FullRunEndToEnd)
{
	TempFile tmp;
	PipelineLogTailer tailer;
	MockObserver observer;

	tailer.Initialize(tmp.Path());
	tailer.RegisterObserver(&observer);

	tmp.Write(kRunStarted);
	tmp.Write(kStageStarted);
	tmp.Write(kLogLine);
	tmp.Write(kStageCompleted);
	tmp.Write(kRunCompleted);
	tailer.Poll();

	EXPECT_EQ(tailer.GetEventCount(), 5);
	EXPECT_FALSE(tailer.IsRunInProgress());

	const RunSummary& summary = tailer.GetCurrentRunSummary();
	EXPECT_EQ(summary.target, Dia::Core::StringCRC("googletest"));
	EXPECT_EQ(summary.config, Dia::Core::StringCRC("Debug"));
	EXPECT_EQ(summary.passCount, 1);
	EXPECT_EQ(summary.failCount, 0);
	EXPECT_EQ(summary.totalDurationMs, 5100);
	EXPECT_FALSE(summary.interrupted);
	EXPECT_GE(observer.notifyCount, 1);

	tailer.UnregisterObserver(&observer);
	tailer.Shutdown();
}
