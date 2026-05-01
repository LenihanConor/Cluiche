#include <gtest/gtest.h>

#include "DiaDebugServer/StateSerializer.h"

using namespace Dia::DebugServer;

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

TEST(StateSerializer, SerializeProcessingUnitState_NullReturnsError)
{
	Json::Value result = StateSerializer::SerializeProcessingUnitState(nullptr);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_STREQ(result["error"].asCString(), "null processing unit");
}

TEST(StateSerializer, SerializePhaseState_NullReturnsError)
{
	Json::Value result = StateSerializer::SerializePhaseState(nullptr);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_STREQ(result["error"].asCString(), "null phase");
}

TEST(StateSerializer, SerializeModuleState_NullReturnsError)
{
	Json::Value result = StateSerializer::SerializeModuleState(nullptr);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_STREQ(result["error"].asCString(), "null module");
}

TEST(StateSerializer, SerializePhaseTransition_AllFieldsPresent)
{
	Dia::Core::StringCRC from("UpdatePhase");
	Dia::Core::StringCRC to("RenderPhase");
	uint64_t timestamp = 1234567890;

	Json::Value result = StateSerializer::SerializePhaseTransition(from, to, timestamp);

	EXPECT_STREQ(result["from_phase"].asCString(), "UpdatePhase");
	EXPECT_STREQ(result["to_phase"].asCString(), "RenderPhase");
	EXPECT_EQ(result["timestamp"].asUInt64(), 1234567890u);
}

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
