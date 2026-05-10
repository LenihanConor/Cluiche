#include <gtest/gtest.h>

#include "DiaDebugServer/StateSerializer.h"
#include "DiaDebugServer/IDebugStateProvider.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::DebugServer;

// -----------------------------------------------------------------------------
// Stub IDebugStateProvider for testing SerializeApplicationState without
// spinning up a real Application.
// -----------------------------------------------------------------------------
struct StubProvider : IDebugStateProvider
{
	Dia::Core::StringCRC currentStage;
	bool transitioning = false;
	bool shuttingDown  = false;

	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
	Dia::Core::Containers::DynamicArrayC<DebugModuleInfo, 64> modules;

	Dia::Core::StringCRC GetCurrentStage() const override { return currentStage; }
	bool IsTransitioning() const override { return transitioning; }
	bool IsShuttingDown() const override { return shuttingDown; }

	void GetProcessingUnitIds(
		Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const override
	{
		for (unsigned int i = 0; i < puIds.Size() && !out.IsFull(); ++i)
			out.Add(puIds[i]);
	}

	void GetModulesInPU(
		const Dia::Core::StringCRC& /*puId*/,
		Dia::Core::Containers::DynamicArrayC<DebugModuleInfo, 64>& out) const override
	{
		for (unsigned int i = 0; i < modules.Size() && !out.IsFull(); ++i)
			out.Add(modules[i]);
	}
};

// -----------------------------------------------------------------------------
// CoreMetrics
// -----------------------------------------------------------------------------

TEST(StateSerializer, SerializeCoreMetrics_AllFieldsPresent)
{
	CoreMetrics metrics;
	metrics.fps = 60.0f;
	metrics.frameTimeMs = 16.67f;
	metrics.memoryUsedMb = 512.0f;
	metrics.memoryAvailableMb = 2048.0f;

	Json::Value result = StateSerializer::SerializeCoreMetrics(metrics);

	EXPECT_FLOAT_EQ(result["fps"].asFloat(), 60.0f);
	EXPECT_FLOAT_EQ(result["frame_time_ms"].asFloat(), 16.67f);
	EXPECT_FLOAT_EQ(result["memory_used_mb"].asFloat(), 512.0f);
	EXPECT_FLOAT_EQ(result["memory_available_mb"].asFloat(), 2048.0f);
}

TEST(StateSerializer, SerializeCoreMetrics_ZeroValues)
{
	CoreMetrics metrics;
	metrics.fps = 0.0f;
	metrics.frameTimeMs = 0.0f;
	metrics.memoryUsedMb = 0.0f;
	metrics.memoryAvailableMb = 0.0f;

	Json::Value result = StateSerializer::SerializeCoreMetrics(metrics);

	EXPECT_FLOAT_EQ(result["fps"].asFloat(), 0.0f);
	EXPECT_FLOAT_EQ(result["frame_time_ms"].asFloat(), 0.0f);
}

// -----------------------------------------------------------------------------
// Application state
// -----------------------------------------------------------------------------

TEST(StateSerializer, SerializeApplicationState_NullReturnsError)
{
	Json::Value result = StateSerializer::SerializeApplicationState(nullptr);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_STREQ(result["error"].asCString(), "null state provider");
}

TEST(StateSerializer, SerializeApplicationState_ReportsCurrentStage)
{
	StubProvider prov;
	prov.currentStage = Dia::Core::StringCRC("RunStage");

	Json::Value result = StateSerializer::SerializeApplicationState(&prov);

	EXPECT_FALSE(result.isMember("error"));
	EXPECT_STREQ(result["current_stage"].asCString(), "RunStage");
	EXPECT_FALSE(result["is_transitioning"].asBool());
	EXPECT_FALSE(result["is_shutting_down"].asBool());
}

TEST(StateSerializer, SerializeApplicationState_IncludesPUsAndModules)
{
	StubProvider prov;
	prov.currentStage = Dia::Core::StringCRC("Boot");
	prov.puIds.Add(Dia::Core::StringCRC("MainPU"));

	DebugModuleInfo mod0;
	mod0.instanceId = Dia::Core::StringCRC("kernel");
	mod0.typeId     = Dia::Core::StringCRC("KernelModule");
	mod0.state      = "active";
	prov.modules.Add(mod0);

	Json::Value result = StateSerializer::SerializeApplicationState(&prov);

	ASSERT_TRUE(result["processing_units"].isArray());
	ASSERT_EQ(result["processing_units"].size(), 1u);
	EXPECT_STREQ(result["processing_units"][0]["pu_id"].asCString(), "MainPU");

	ASSERT_TRUE(result["processing_units"][0]["modules"].isArray());
	ASSERT_EQ(result["processing_units"][0]["modules"].size(), 1u);
	EXPECT_STREQ(result["processing_units"][0]["modules"][0]["module_id"].asCString(), "kernel");
	EXPECT_STREQ(result["processing_units"][0]["modules"][0]["type_id"].asCString(), "KernelModule");
	EXPECT_STREQ(result["processing_units"][0]["modules"][0]["state"].asCString(), "active");
}

// -----------------------------------------------------------------------------
// Per-module
// -----------------------------------------------------------------------------

TEST(StateSerializer, SerializeModuleState_PassesThroughStateString)
{
	DebugModuleInfo info;
	info.instanceId = Dia::Core::StringCRC("mod");
	info.typeId     = Dia::Core::StringCRC("Type");

	const char* states[] = { "inactive", "starting", "active", "stopping", "failed" };

	for (unsigned int i = 0; i < sizeof(states)/sizeof(states[0]); ++i)
	{
		info.state = states[i];
		Json::Value result = StateSerializer::SerializeModuleState(info);
		EXPECT_STREQ(result["state"].asCString(), states[i]) << "state index " << i;
	}
}

TEST(StateSerializer, SerializeModuleState_NullStateStringBecomesUnknown)
{
	DebugModuleInfo info;
	info.instanceId = Dia::Core::StringCRC("mod");
	info.typeId     = Dia::Core::StringCRC("Type");
	info.state      = nullptr;

	Json::Value result = StateSerializer::SerializeModuleState(info);
	EXPECT_STREQ(result["state"].asCString(), "unknown");
}

// -----------------------------------------------------------------------------
// Stage transition (replaces v1 phase transition)
// -----------------------------------------------------------------------------

TEST(StateSerializer, SerializeStageTransition_AllFieldsPresent)
{
	Dia::Core::StringCRC from("Boot");
	Dia::Core::StringCRC to("Run");
	uint64_t timestamp = 1234567890;

	Json::Value result = StateSerializer::SerializeStageTransition(from, to, timestamp);

	EXPECT_STREQ(result["from_stage"].asCString(), "Boot");
	EXPECT_STREQ(result["to_stage"].asCString(), "Run");
	EXPECT_EQ(result["timestamp"].asUInt64(), 1234567890u);
}

// -----------------------------------------------------------------------------
// Command response
// -----------------------------------------------------------------------------

TEST(StateSerializer, SerializeCommandResponse_Success)
{
	Dia::Core::StringCRC cmd("hot_reload");
	Json::Value result = StateSerializer::SerializeCommandResponse(cmd, true, "Reload complete");

	EXPECT_STREQ(result["command"].asCString(), "hot_reload");
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_STREQ(result["message"].asCString(), "Reload complete");
}

TEST(StateSerializer, SerializeCommandResponse_Failure)
{
	Dia::Core::StringCRC cmd("compile_asset");
	Json::Value result = StateSerializer::SerializeCommandResponse(cmd, false, "File not found");

	EXPECT_FALSE(result["success"].asBool());
	EXPECT_STREQ(result["message"].asCString(), "File not found");
}

TEST(StateSerializer, SerializeCommandResponse_NullMessage)
{
	Dia::Core::StringCRC cmd("test");
	Json::Value result = StateSerializer::SerializeCommandResponse(cmd, true, nullptr);

	EXPECT_STREQ(result["message"].asCString(), "");
}
