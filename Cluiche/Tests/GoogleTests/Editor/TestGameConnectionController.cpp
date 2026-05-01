#include <gtest/gtest.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace Dia::Editor;

// --- Construction & Initial State ---

TEST(GameConnectionController, InitiallyDisconnected)
{
	GameConnectionController controller;
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, InitialUrlIsEmpty)
{
	GameConnectionController controller;
	EXPECT_STREQ(controller.GetUrl(), "");
}

TEST(GameConnectionController, InitialLastErrorIsEmpty)
{
	GameConnectionController controller;
	EXPECT_STREQ(controller.GetLastError(), "");
}

// --- Initialize / Shutdown Lifecycle ---

TEST(GameConnectionController, InitializeWithNullBridgeAndManager)
{
	GameConnectionController controller;
	controller.Initialize(nullptr, nullptr);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, InitializeWithBridgeOnly)
{
	WebUIBridge bridge(nullptr);
	GameConnectionController controller;
	controller.Initialize(&bridge, nullptr);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, InitializeWithManagerOnly)
{
	GameConnectionManager manager;
	GameConnectionController controller;
	controller.Initialize(nullptr, &manager);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, InitializeWithBothBridgeAndManager)
{
	WebUIBridge bridge(nullptr);
	GameConnectionManager manager;
	GameConnectionController controller;
	controller.Initialize(&bridge, &manager);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, ShutdownFromInitialState)
{
	GameConnectionController controller;
	controller.Shutdown();
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, DoubleShutdownIsSafe)
{
	WebUIBridge bridge(nullptr);
	GameConnectionManager manager;
	GameConnectionController controller;
	controller.Initialize(&bridge, &manager);
	controller.Shutdown();
	controller.Shutdown();
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, InitializeShutdownCycle)
{
	WebUIBridge bridge(nullptr);
	GameConnectionManager manager;
	GameConnectionController controller;

	controller.Initialize(&bridge, &manager);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
	controller.Shutdown();
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);

	controller.Initialize(&bridge, &manager);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
	controller.Shutdown();
}

// --- Update (no active connection) ---

TEST(GameConnectionController, UpdateWhileDisconnectedIsNoop)
{
	GameConnectionController controller;
	controller.Update(0.016f);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST(GameConnectionController, UpdateWithInitializedControllerIsNoop)
{
	WebUIBridge bridge(nullptr);
	GameConnectionManager manager;
	GameConnectionController controller;
	controller.Initialize(&bridge, &manager);

	for (int i = 0; i < 100; ++i)
		controller.Update(0.016f);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

// --- Persistence ---

class GameConnectionControllerPersistence : public ::testing::Test
{
protected:
	std::string mTempPath;

	void SetUp() override
	{
		char* tmpDir = nullptr;
		size_t len = 0;
		_dupenv_s(&tmpDir, &len, "TEMP");
		if (tmpDir == nullptr)
			_dupenv_s(&tmpDir, &len, "TMP");
		mTempPath = std::string(tmpDir ? tmpDir : ".") + "\\dia_test_gc_persist.json";
		free(tmpDir);

		std::remove(mTempPath.c_str());
	}

	void TearDown() override
	{
		std::remove(mTempPath.c_str());
	}
};

TEST_F(GameConnectionControllerPersistence, SetPersistencePathNull)
{
	GameConnectionController controller;
	controller.SetPersistencePath(nullptr);
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
}

TEST_F(GameConnectionControllerPersistence, LoadPersistedUrlWithNoPath)
{
	GameConnectionController controller;
	controller.LoadPersistedUrl();
	EXPECT_STREQ(controller.GetUrl(), "");
}

TEST_F(GameConnectionControllerPersistence, SavePersistedUrlWithEmptyUrl)
{
	GameConnectionController controller;
	controller.SetPersistencePath(mTempPath.c_str());
	controller.SavePersistedUrl();

	std::ifstream f(mTempPath);
	EXPECT_FALSE(f.is_open());
}

TEST_F(GameConnectionControllerPersistence, LoadPersistedUrlFromValidFile)
{
	{
		std::ofstream out(mTempPath);
		out << R"({"url":"ws://testhost:1234"})";
	}

	GameConnectionController controller;
	controller.SetPersistencePath(mTempPath.c_str());
	controller.LoadPersistedUrl();
	EXPECT_STREQ(controller.GetUrl(), "ws://testhost:1234");
}

TEST_F(GameConnectionControllerPersistence, LoadPersistedUrlFromMissingFile)
{
	GameConnectionController controller;
	controller.SetPersistencePath(mTempPath.c_str());
	controller.LoadPersistedUrl();
	EXPECT_STREQ(controller.GetUrl(), "");
}

TEST_F(GameConnectionControllerPersistence, LoadPersistedUrlFromMalformedJson)
{
	{
		std::ofstream out(mTempPath);
		out << "not valid json {{{";
	}

	GameConnectionController controller;
	controller.SetPersistencePath(mTempPath.c_str());
	controller.LoadPersistedUrl();
	EXPECT_STREQ(controller.GetUrl(), "");
}

TEST_F(GameConnectionControllerPersistence, LoadPersistedUrlFromJsonMissingUrlField)
{
	{
		std::ofstream out(mTempPath);
		out << R"({"other":"value"})";
	}

	GameConnectionController controller;
	controller.SetPersistencePath(mTempPath.c_str());
	controller.LoadPersistedUrl();
	EXPECT_STREQ(controller.GetUrl(), "");
}

TEST_F(GameConnectionControllerPersistence, LoadPersistedUrlPreservesStateAsDisconnected)
{
	{
		std::ofstream out(mTempPath);
		out << R"({"url":"ws://localhost:9002"})";
	}

	GameConnectionController controller;
	controller.SetPersistencePath(mTempPath.c_str());
	controller.LoadPersistedUrl();
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
	EXPECT_STREQ(controller.GetUrl(), "ws://localhost:9002");
}
