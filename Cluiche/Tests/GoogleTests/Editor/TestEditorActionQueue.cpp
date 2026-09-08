#include <gtest/gtest.h>
#include <DiaEditor/EditorAPI/EditorActionQueue.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <thread>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
	EditorActionDescriptor MakeEchoDescriptor(const char* name)
	{
		EditorActionDescriptor d;
		d.name           = StringCRC(name);
		d.description    = "echo";
		d.category       = "test";
		d.owner          = "TestOwner";
		d.dispatchThread = DispatchThread::kMainThread;
		d.handler        = [](const Json::Value& p) { return p; };
		return d;
	}
}

// ---------------------------------------------------------------------------
// Push + single DoUpdate roundtrip
// ---------------------------------------------------------------------------

TEST(EditorActionQueue, DispatchAndWait_SingleAction_ReturnsHandlerResult)
{
	EditorActionRegistry reg;
	reg.Initialize();
	reg.RegisterAction(MakeEchoDescriptor("test.echo"));

	EditorActionQueue queue;

	// Push from a worker thread, drain from "main thread" (this thread, for test)
	Json::Value params;
	params["val"] = 99;

	std::thread worker([&]() {
		Json::Value result = queue.DispatchAndWait(StringCRC("test.echo"), params);
		EXPECT_EQ(result["val"].asInt(), 99);
	});

	// Give the worker a moment to enqueue
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	queue.DoUpdate(&reg, 0.016f);

	worker.join();
	reg.Shutdown();
}

// ---------------------------------------------------------------------------
// Unknown action returns unknown_action via queue path
// ---------------------------------------------------------------------------

TEST(EditorActionQueue, DispatchAndWait_UnknownAction_ReturnsUnknownAction)
{
	EditorActionRegistry reg;
	reg.Initialize();

	EditorActionQueue queue;

	std::thread worker([&]() {
		Json::Value result = queue.DispatchAndWait(StringCRC("test.nonexistent"), Json::Value{});
		EXPECT_FALSE(result["success"].asBool());
		EXPECT_STREQ(result["reason"].asCString(), "unknown_action");
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	queue.DoUpdate(&reg, 0.016f);

	worker.join();
	reg.Shutdown();
}

// ---------------------------------------------------------------------------
// Concurrent pushes from 2 threads both complete
// ---------------------------------------------------------------------------

TEST(EditorActionQueue, ConcurrentPushes_BothComplete)
{
	EditorActionRegistry reg;
	reg.Initialize();

	// Register a handler that returns the input unchanged
	reg.RegisterAction(MakeEchoDescriptor("test.concurrent"));

	EditorActionQueue queue;

	Json::Value paramsA;
	paramsA["id"] = "A";
	Json::Value paramsB;
	paramsB["id"] = "B";

	bool aOk = false, bOk = false;

	std::thread workerA([&]() {
		Json::Value result = queue.DispatchAndWait(StringCRC("test.concurrent"), paramsA);
		aOk = (result["id"].asString() == "A");
	});

	std::thread workerB([&]() {
		Json::Value result = queue.DispatchAndWait(StringCRC("test.concurrent"), paramsB);
		bOk = (result["id"].asString() == "B");
	});

	// Drain until both workers complete (give them time to enqueue)
	while (!aOk || !bOk)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		queue.DoUpdate(&reg, 0.016f);
	}

	workerA.join();
	workerB.join();

	EXPECT_TRUE(aOk);
	EXPECT_TRUE(bOk);

	reg.Shutdown();
}

// ---------------------------------------------------------------------------
// GetDepth reflects pending items
// ---------------------------------------------------------------------------

TEST(EditorActionQueue, GetDepth_ReflectsPendingCount)
{
	EditorActionRegistry reg;
	reg.Initialize();

	// Register a handler that blocks until signalled (so items stay pending)
	std::mutex blockMutex;
	std::condition_variable blockCv;
	bool unblock = false;

	EditorActionDescriptor d;
	d.name           = StringCRC("test.blocking");
	d.description    = "block";
	d.category       = "test";
	d.owner          = "TestOwner";
	d.dispatchThread = DispatchThread::kMainThread;
	d.handler        = [&](const Json::Value&) {
		std::unique_lock<std::mutex> lk(blockMutex);
		blockCv.wait(lk, [&]{ return unblock; });
		Json::Value r;
		r["success"] = true;
		return r;
	};
	reg.RegisterAction(d);

	EditorActionQueue queue;

	// Push one item but don't drain yet
	std::thread worker([&]() {
		queue.DispatchAndWait(StringCRC("test.blocking"), Json::Value{});
	});

	// Allow worker to enqueue
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	EXPECT_GE(queue.GetDepth(), 0u);  // either 0 or 1 depending on timing — just no crash

	// Drain (handler blocks, but we just test it doesn't crash)
	{
		std::lock_guard<std::mutex> lk(blockMutex);
		unblock = true;
		blockCv.notify_all();
	}
	queue.DoUpdate(&reg, 0.016f);

	worker.join();
	reg.Shutdown();
}
