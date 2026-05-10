#define _CRT_SECURE_NO_WARNINGS
#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaApplicationFlow/ApplicationError.h>
#include <DiaApplicationFlow/Loader/ApplicationLoader.h>
#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>

#include <DiaCore/Json/external/json/json.h>

#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>

using namespace Dia::Application;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Test PU that runs on a thread and counts updates
// ---------------------------------------------------------------------------

class ThreadedTestPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;

	ThreadedTestPU(const StringCRC& id, float hz = 60.0f)
		: ProcessingUnit(id, hz, 4, 4)
		, mUpdateCount(0)
		, mStarted(false)
		, mStopped(false)
		, mStopFlag(false)
	{}

	bool FlaggedToStopUpdating() const override { return mStopFlag.load(); }
	void SignalStop() { mStopFlag.store(true); }

	std::atomic<int> mUpdateCount;
	std::atomic<bool> mStarted;
	std::atomic<bool> mStopped;
	std::atomic<bool> mStopFlag;

protected:
	void PostPhaseStart(const IStartData* startData) override
	{
		mStarted.store(true);
	}

	void PrePhaseUpdate() override
	{
		mUpdateCount.fetch_add(1);
	}

	void PrePhaseStop() override
	{
		mStopped.store(true);
	}
};
const StringCRC ThreadedTestPU::kTypeId("ThreadedTestPU");

// ---------------------------------------------------------------------------
// Minimal phase
// ---------------------------------------------------------------------------

class IntegrationTestPhase : public Phase
{
public:
	static const StringCRC kTypeId;
	IntegrationTestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC IntegrationTestPhase::kTypeId("IntegrationTestPhase");

// ---------------------------------------------------------------------------
// Parent PU that manages children via tree
// ---------------------------------------------------------------------------

class ParentTestPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;

	ParentTestPU(const StringCRC& id)
		: ProcessingUnit(id, -1.0f, 4, 4)
		, mStopFlag(false)
	{}

	bool FlaggedToStopUpdating() const override { return mStopFlag.load(); }
	void SignalStop() { mStopFlag.store(true); }

	std::atomic<bool> mStopFlag;
};
const StringCRC ParentTestPU::kTypeId("ParentTestPU");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Dia::Core::UniquePtr<ProcessingUnit> MakeThreadedChild(const StringCRC& id, float hz = 60.0f)
{
	return Dia::Core::UniquePtr<ProcessingUnit>(new ThreadedTestPU(id, hz));
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class PUTreeIntegrationTest : public ::testing::Test
{
protected:
};

// ---------------------------------------------------------------------------
// AC6: Parent spawns threads for dedicated children during Start
// ---------------------------------------------------------------------------
TEST_F(PUTreeIntegrationTest, AutoThreadSpawn_ChildrenRunOnSeparateThreads)
{
	ParentTestPU root(StringCRC("Root"));

	auto child = MakeThreadedChild(StringCRC("ChildA"), 100.0f);
	ThreadedTestPU* childPtr = static_cast<ThreadedTestPU*>(child.Get());

	// Set up child with a phase so it can start
	IntegrationTestPhase childPhase(childPtr, StringCRC("ChildPhase"));
	childPtr->AddPhase(&childPhase);
	childPtr->SetInitialPhase(&childPhase);

	root.AddChildProcessingUnit(std::move(child));

	// Set up root with a phase
	IntegrationTestPhase rootPhase(&root, StringCRC("RootPhase"));
	root.AddPhase(&rootPhase);
	root.SetInitialPhase(&rootPhase);

	root.Initialize();
	root.Start(nullptr);

	// Wait for child to start updating
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (childPtr->mUpdateCount.load() < 3 && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	EXPECT_TRUE(childPtr->mStarted.load());
	EXPECT_GE(childPtr->mUpdateCount.load(), 3);

	// Stop everything
	childPtr->SignalStop();
	root.SignalStop();
	root.Stop();

	EXPECT_TRUE(childPtr->mStopped.load());
}

// ---------------------------------------------------------------------------
// AC7: Parent joins child threads during Stop (children stop before parent)
// ---------------------------------------------------------------------------
TEST_F(PUTreeIntegrationTest, AutoThreadJoin_ChildrenStopBeforeParent)
{
	ParentTestPU root(StringCRC("Root"));

	auto child = MakeThreadedChild(StringCRC("ChildA"), 100.0f);
	ThreadedTestPU* childPtr = static_cast<ThreadedTestPU*>(child.Get());

	IntegrationTestPhase childPhase(childPtr, StringCRC("ChildPhase"));
	childPtr->AddPhase(&childPhase);
	childPtr->SetInitialPhase(&childPhase);

	root.AddChildProcessingUnit(std::move(child));

	IntegrationTestPhase rootPhase(&root, StringCRC("RootPhase"));
	root.AddPhase(&rootPhase);
	root.SetInitialPhase(&rootPhase);

	root.Initialize();
	root.Start(nullptr);

	// Let it run briefly
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Stop triggers child stop + join before parent stops
	root.Stop();

	// After Stop() returns, child must be stopped
	EXPECT_TRUE(childPtr->mStopped.load());
}

// ---------------------------------------------------------------------------
// AC14: Child PU error propagation to parent
// ---------------------------------------------------------------------------
static std::atomic<bool> sErrorReceived{false};
static void ErrorCallbackForTest(const ErrorInfo& error)
{
	sErrorReceived.store(true);
}

TEST_F(PUTreeIntegrationTest, ChildError_PropagatedToParent)
{
	sErrorReceived.store(false);

	ParentTestPU root(StringCRC("Root"));
	root.SetErrorCallback(&ErrorCallbackForTest);

	auto child = MakeThreadedChild(StringCRC("ErrorChild"), 100.0f);
	ThreadedTestPU* childPtr = static_cast<ThreadedTestPU*>(child.Get());

	IntegrationTestPhase childPhase(childPtr, StringCRC("ChildPhase"));
	childPtr->AddPhase(&childPhase);
	childPtr->SetInitialPhase(&childPhase);

	root.AddChildProcessingUnit(std::move(child));

	IntegrationTestPhase rootPhase(&root, StringCRC("RootPhase"));
	root.AddPhase(&rootPhase);
	root.SetInitialPhase(&rootPhase);

	root.Initialize();
	root.Start(nullptr);

	// Wait for child to start
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	while (!childPtr->mStarted.load() && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	// Simulate child reporting an error — parent callback should fire
	ErrorInfo childError;
	childError.code = ErrorCode::kUnknown;
	childPtr->ReportError(childError);

	EXPECT_TRUE(sErrorReceived.load());

	childPtr->SignalStop();
	root.Stop();
}

// ---------------------------------------------------------------------------
// File helpers for manifest-based tests
// ---------------------------------------------------------------------------

static void WriteFile(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	if (f) { fputs(content, f); fclose(f); }
}

static void DeleteTestFile(const char* path) { remove(path); }

// Registrable test PU for LoadApplicationTree tests
class TreeLoadTestPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;
	TreeLoadTestPU(const StringCRC& id, float hz) : ProcessingUnit(id, hz, 4, 4) {}
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC TreeLoadTestPU::kTypeId("TreeLoadTestPU");

class TreeLoadTestPhase : public Phase
{
public:
	static const StringCRC kTypeId;
	TreeLoadTestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC TreeLoadTestPhase::kTypeId("TreeLoadTestPhase");

class TreeLoadPUFactory : public ITypeFactory<ProcessingUnit>
{
public:
	ProcessingUnit* Create(const Dia::Core::StringCRC& instanceId, const Json::Value& config) override
	{
		float hz = config.get("frequency_hz", -1.0f).asFloat();
		return new TreeLoadTestPU(instanceId, hz);
	}
};

class TreeLoadPhaseFactory : public ITypeFactory<Phase>
{
public:
	Phase* Create(ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) override
	{
		return new TreeLoadTestPhase(pu, instanceId);
	}
};

class PUTreeLoaderTest : public ::testing::Test
{
protected:
	ApplicationTypeRegistry mRegistry;
	TreeLoadPUFactory mPUFactory;
	TreeLoadPhaseFactory mPhaseFactory;

	void SetUp() override
	{
		mRegistry.RegisterProcessingUnitType(TreeLoadTestPU::kTypeId, &mPUFactory);
		mRegistry.RegisterPhaseType(TreeLoadTestPhase::kTypeId, &mPhaseFactory);
	}
};

// ---------------------------------------------------------------------------
// AC11: LoadApplicationTree builds tree from merged manifest (3 PUs)
// ---------------------------------------------------------------------------
TEST_F(PUTreeLoaderTest, LoadApplicationTree_ThreePUs_BuildsTree)
{
	const char* manifest =
		"{ \"version\": 1, \"processing_units\": ["
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"RootPU\","
		"    \"frequency_hz\": -1.0, \"dedicated_thread\": false, \"root\": true,"
		"    \"initial_phase\": \"RootPhase\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"RootPhase\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} },"
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"SimPU\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": true,"
		"    \"initial_phase\": \"SimPhase\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"SimPhase\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} },"
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"RenderPU\","
		"    \"frequency_hz\": 60.0, \"dedicated_thread\": true,"
		"    \"initial_phase\": \"RenderPhase\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"RenderPhase\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} }"
		"] }";

	WriteFile("_test_tree_3pu.diaapp", manifest);

	ManifestValidationResult result;
	ProcessingUnit* root = ApplicationLoader::LoadApplicationTree(mRegistry, "_test_tree_3pu.diaapp", result);

	ASSERT_NE(root, nullptr);
	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(root->GetUniqueId(), StringCRC("RootPU"));
	EXPECT_TRUE(root->IsRoot());
	EXPECT_EQ(root->GetChildren().Size(), 2u);
	EXPECT_NE(root->FindChildProcessingUnit(StringCRC("SimPU")), nullptr);
	EXPECT_NE(root->FindChildProcessingUnit(StringCRC("RenderPU")), nullptr);

	// Tree traversal works
	EXPECT_EQ(root->FindProcessingUnitInTree(StringCRC("SimPU")),
			  root->FindChildProcessingUnit(StringCRC("SimPU")));

	delete root;
	DeleteTestFile("_test_tree_3pu.diaapp");
}

// ---------------------------------------------------------------------------
// AC15: Exactly one root required — zero roots rejected
// ---------------------------------------------------------------------------
TEST_F(PUTreeLoaderTest, LoadApplicationTree_ZeroRoots_Rejected)
{
	const char* manifest =
		"{ \"version\": 1, \"processing_units\": ["
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"PU_A\","
		"    \"frequency_hz\": -1.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} }"
		"] }";

	WriteFile("_test_tree_noroot.diaapp", manifest);

	ManifestValidationResult result;
	ProcessingUnit* root = ApplicationLoader::LoadApplicationTree(mRegistry, "_test_tree_noroot.diaapp", result);

	EXPECT_EQ(root, nullptr);
	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);

	DeleteTestFile("_test_tree_noroot.diaapp");
}

// ---------------------------------------------------------------------------
// AC15: Exactly one root required — multiple roots rejected
// ---------------------------------------------------------------------------
TEST_F(PUTreeLoaderTest, LoadApplicationTree_MultipleRoots_Rejected)
{
	const char* manifest =
		"{ \"version\": 1, \"processing_units\": ["
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"PU_A\","
		"    \"frequency_hz\": -1.0, \"dedicated_thread\": false, \"root\": true,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} },"
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"PU_B\","
		"    \"frequency_hz\": -1.0, \"dedicated_thread\": false, \"root\": true,"
		"    \"initial_phase\": \"PhaseB\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"PhaseB\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} }"
		"] }";

	WriteFile("_test_tree_multiroot.diaapp", manifest);

	ManifestValidationResult result;
	ProcessingUnit* root = ApplicationLoader::LoadApplicationTree(mRegistry, "_test_tree_multiroot.diaapp", result);

	EXPECT_EQ(root, nullptr);
	EXPECT_NE(result, ManifestValidationResult::kSuccess);

	DeleteTestFile("_test_tree_multiroot.diaapp");
}

// ---------------------------------------------------------------------------
// AC15: Exactly one root — single root succeeds
// ---------------------------------------------------------------------------
TEST_F(PUTreeLoaderTest, LoadApplicationTree_SingleRoot_Succeeds)
{
	const char* manifest =
		"{ \"version\": 1, \"processing_units\": ["
		"  { \"type_id\": \"TreeLoadTestPU\", \"instance_id\": \"OnlyPU\","
		"    \"frequency_hz\": -1.0, \"dedicated_thread\": false, \"root\": true,"
		"    \"initial_phase\": \"OnlyPhase\","
		"    \"phases\": [{ \"type_id\": \"TreeLoadTestPhase\", \"instance_id\": \"OnlyPhase\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {} }"
		"] }";

	WriteFile("_test_tree_singleroot.diaapp", manifest);

	ManifestValidationResult result;
	ProcessingUnit* root = ApplicationLoader::LoadApplicationTree(mRegistry, "_test_tree_singleroot.diaapp", result);

	ASSERT_NE(root, nullptr);
	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(root->GetUniqueId(), StringCRC("OnlyPU"));
	EXPECT_TRUE(root->IsRoot());
	EXPECT_EQ(root->GetChildren().Size(), 0u);

	delete root;
	DeleteTestFile("_test_tree_singleroot.diaapp");
}

// ---------------------------------------------------------------------------
// AC15 (original): File not found rejected
// ---------------------------------------------------------------------------
TEST_F(PUTreeIntegrationTest, LoadApplicationTree_FileNotFound_Rejected)
{
	ApplicationTypeRegistry registry;
	ManifestValidationResult result;

	ProcessingUnit* pu = ApplicationLoader::LoadApplicationTree(registry, "nonexistent.diaapp", result);
	EXPECT_EQ(pu, nullptr);
	EXPECT_NE(result, ManifestValidationResult::kSuccess);
}
