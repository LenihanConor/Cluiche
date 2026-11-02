#include <gtest/gtest.h>
#include <Modules/TestStages/CellLayersPointerQueue.h>

using namespace CluicheTest;

namespace
{
	Json::Value Params(const char* text)
	{
		Json::Value v;
		Json::Reader reader;
		EXPECT_TRUE(reader.parse(text, v));
		return v;
	}

	std::vector<PointerFrameQueue::Frame> Frames(size_t n, float firstX = 0.0f)
	{
		std::vector<PointerFrameQueue::Frame> out(n);
		for (size_t i = 0; i < n; ++i) { out[i].x = firstX + static_cast<float>(i); out[i].valid = true; }
		return out;
	}
}

TEST(CellLayersPointerQueueTest, ParseValidFramesAppliesDefaultsAndFlags)
{
	std::vector<PointerFrameQueue::Frame> f;
	std::string err;
	ASSERT_TRUE(ParsePointerFrames(Params(R"({"frames":[{"x":1.5,"y":2},{"x":3,"y":4,"left":true,"shift":true,"esc":true}]})"), f, err)) << err;
	ASSERT_EQ(f.size(), 2u);
	EXPECT_FLOAT_EQ(f[0].x, 1.5f);
	EXPECT_FALSE(f[0].left);
	EXPECT_FALSE(f[0].shift);
	EXPECT_TRUE(f[0].valid);
	EXPECT_TRUE(f[0].hasKeyboard);
	EXPECT_TRUE(f[1].left);
	EXPECT_TRUE(f[1].shift);
	EXPECT_TRUE(f[1].escPressed);
	EXPECT_FALSE(f[1].ctrl);
}

TEST(CellLayersPointerQueueTest, ParseRejectsBadInput)
{
	std::vector<PointerFrameQueue::Frame> f;
	std::string err;
	EXPECT_FALSE(ParsePointerFrames(Params("{}"), f, err));
	EXPECT_EQ(err, "missing or non-array param 'frames'");
	EXPECT_FALSE(ParsePointerFrames(Params(R"({"frames":5})"), f, err));
	EXPECT_EQ(err, "missing or non-array param 'frames'");
	EXPECT_FALSE(ParsePointerFrames(Params(R"({"frames":[]})"), f, err));
	EXPECT_FALSE(ParsePointerFrames(Params(R"({"frames":[{"x":1,"y":1},{"y":1}]})"), f, err));
	EXPECT_EQ(err, "frame 1: missing or non-numeric 'x'");
	EXPECT_FALSE(ParsePointerFrames(Params(R"({"frames":[{"x":1,"y":"a"}]})"), f, err));
	EXPECT_EQ(err, "frame 0: missing or non-numeric 'y'");
	EXPECT_FALSE(ParsePointerFrames(Params(R"({"frames":[{"x":1,"y":1,"left":1}]})"), f, err));
	EXPECT_EQ(err, "frame 0: non-bool 'left'");
	EXPECT_FALSE(ParsePointerFrames(Params(R"({"frames":[3]})"), f, err));
	EXPECT_TRUE(f.empty());
}

TEST(CellLayersPointerQueueTest, ParseEnforcesPerCallLimit)
{
	Json::Value p(Json::objectValue);
	Json::Value arr(Json::arrayValue);
	for (size_t i = 0; i < kPointerMaxFramesPerCall; ++i)
	{
		Json::Value fr(Json::objectValue);
		fr["x"] = 1; fr["y"] = 1;
		arr.append(fr);
	}
	p["frames"] = arr;
	std::vector<PointerFrameQueue::Frame> f;
	std::string err;
	EXPECT_TRUE(ParsePointerFrames(p, f, err)) << err;
	Json::Value extra(Json::objectValue);
	extra["x"] = 1; extra["y"] = 1;
	p["frames"].append(extra);
	EXPECT_FALSE(ParsePointerFrames(p, f, err));
	EXPECT_NE(err.find("too many"), std::string::npos);
}

TEST(CellLayersPointerQueueTest, PopsOneAtATimeInOrder)
{
	PointerFrameQueue q;
	ASSERT_TRUE(q.TryEnqueue(Frames(3, 10.0f)));
	EXPECT_EQ(q.Pending(), 3u);
	PointerFrameQueue::Frame f;
	for (int i = 0; i < 3; ++i)
	{
		ASSERT_TRUE(q.TryPop(f));
		EXPECT_FLOAT_EQ(f.x, 10.0f + static_cast<float>(i));
		EXPECT_EQ(q.Pending(), static_cast<size_t>(2 - i));
	}
	EXPECT_FALSE(q.TryPop(f));
}

TEST(CellLayersPointerQueueTest, BatchRejectedAsWholeWhenFull)
{
	PointerFrameQueue q;
	ASSERT_TRUE(q.TryEnqueue(Frames(kPointerQueueCapacity - 2)));
	EXPECT_FALSE(q.TryEnqueue(Frames(3)));
	EXPECT_EQ(q.Pending(), kPointerQueueCapacity - 2);
	EXPECT_TRUE(q.TryEnqueue(Frames(2)));
	EXPECT_EQ(q.Pending(), kPointerQueueCapacity);
	EXPECT_FALSE(q.TryEnqueue(Frames(1)));
}

TEST(CellLayersPointerQueueTest, CapacityIsReclaimedAfterPopsAndClear)
{
	PointerFrameQueue q;
	ASSERT_TRUE(q.TryEnqueue(Frames(kPointerQueueCapacity)));
	PointerFrameQueue::Frame f;
	ASSERT_TRUE(q.TryPop(f));
	ASSERT_TRUE(q.TryPop(f));
	EXPECT_TRUE(q.TryEnqueue(Frames(2, 100.0f)));
	EXPECT_EQ(q.Pending(), kPointerQueueCapacity);
	q.Clear();
	EXPECT_EQ(q.Pending(), 0u);
	EXPECT_FALSE(q.TryPop(f));
}

// Streams Migration Task 5: the queue is plain (push and pop both on SimPU) and the three test commands are sim-hosted.
#ifdef DIA_DEBUG
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaAPI/DiaAPI.h>

namespace
{
	const char* const kInjectionNames[] = {
		"cluichetest.celllayers.pointer_frames",
		"cluichetest.celllayers.pointer_pending",
		"cluichetest.celllayers.cell_pixel"
	};

	struct PointerInjectionCommandsTest : ::testing::Test
	{
		void SetUp() override { Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	Dia::CellLayers::MirrorQueryHandler PixelStub()
	{
		return [](const Json::Value&) { Json::Value o(Json::objectValue); o["x"] = 1.0; o["y"] = 2.0; return o; };
	}
}

TEST_F(PointerInjectionCommandsTest, RegistersThreeCommandsThroughTheRegistrarNotTheDiaApi)
{
	PointerFrameQueue q;
	Dia::CellLayers::Testing::FakeMirrorRegistrar reg;
	ASSERT_TRUE(RegisterPointerInjectionCommands(reg, q, PixelStub()));
	EXPECT_EQ(reg.Count(), 3u);
	for (const char* n : kInjectionNames)
	{
		EXPECT_TRUE(reg.Has(n)) << n;
		EXPECT_EQ(Dia::API::GetCommand(Dia::Core::StringCRC(n)), nullptr) << n;
		EXPECT_FALSE(Dia::API::ExecuteCommandJson(Dia::Core::StringCRC(n), Json::Value(Json::objectValue))["success"].asBool()) << n;
	}
	EXPECT_TRUE(reg.IsCommand("cluichetest.celllayers.pointer_frames"));
	UnregisterPointerInjectionCommands(reg);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST_F(PointerInjectionCommandsTest, HandlersEnqueueAndReportPending)
{
	PointerFrameQueue q;
	Dia::CellLayers::Testing::FakeMirrorRegistrar reg;
	ASSERT_TRUE(RegisterPointerInjectionCommands(reg, q, PixelStub()));
	Json::Value r = reg.Execute("cluichetest.celllayers.pointer_frames", Params(R"({"frames":[{"x":1,"y":2},{"x":3,"y":4}]})"));
	EXPECT_TRUE(r["ok"].asBool());
	EXPECT_EQ(r["queued"].asUInt(), 2u);
	EXPECT_EQ(r["pending"].asUInt(), 2u);
	EXPECT_EQ(q.Pending(), 2u);
	EXPECT_EQ(reg.Execute("cluichetest.celllayers.pointer_pending", Json::Value(Json::objectValue))["pending"].asUInt(), 2u);
	EXPECT_EQ(reg.Execute("cluichetest.celllayers.cell_pixel", Json::Value(Json::objectValue))["x"].asDouble(), 1.0);
	EXPECT_EQ(reg.Execute("cluichetest.celllayers.pointer_frames", Params("{}"))["error"].asString(), "missing or non-array param 'frames'");
	UnregisterPointerInjectionCommands(reg);
}

TEST_F(PointerInjectionCommandsTest, QueueFullIsReportedAsError)
{
	PointerFrameQueue q;
	ASSERT_TRUE(q.TryEnqueue(Frames(kPointerQueueCapacity)));
	Dia::CellLayers::Testing::FakeMirrorRegistrar reg;
	ASSERT_TRUE(RegisterPointerInjectionCommands(reg, q, PixelStub()));
	EXPECT_EQ(reg.Execute("cluichetest.celllayers.pointer_frames", Params(R"({"frames":[{"x":1,"y":2}]})"))["error"].asString(), "queue full");
	UnregisterPointerInjectionCommands(reg);
}
#endif
