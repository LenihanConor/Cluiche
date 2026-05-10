#include <gtest/gtest.h>
#include <DiaApplicationFlow/MessageBus.h>

using namespace Dia::Application;
using namespace Dia::Core;

TEST(MessageBus, InitialState)
{
	MessageBus bus;

	EXPECT_EQ(bus.GetQueuedMessageCount(), 0u);
	EXPECT_EQ(bus.GetSubscriberCount(StringCRC("TestMsg")), 0u);
}

TEST(MessageBus, SubscribeIncreasesCount)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");

	bus.Subscribe(msgType, StringCRC("Sub1"), [](const Message&) {});
	EXPECT_EQ(bus.GetSubscriberCount(msgType), 1u);

	bus.Subscribe(msgType, StringCRC("Sub2"), [](const Message&) {});
	EXPECT_EQ(bus.GetSubscriberCount(msgType), 2u);
}

TEST(MessageBus, UnsubscribeRemovesHandler)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");
	StringCRC subId("Sub1");

	bus.Subscribe(msgType, subId, [](const Message&) {});
	EXPECT_EQ(bus.GetSubscriberCount(msgType), 1u);

	bus.Unsubscribe(msgType, subId);
	EXPECT_EQ(bus.GetSubscriberCount(msgType), 0u);
}

TEST(MessageBus, UnsubscribeNonexistentIsNoop)
{
	MessageBus bus;
	bus.Unsubscribe(StringCRC("TestMsg"), StringCRC("Nobody"));
	EXPECT_EQ(bus.GetSubscriberCount(StringCRC("TestMsg")), 0u);
}

TEST(MessageBus, SendImmediateCallsHandler)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");
	int receivedValue = 0;

	bus.Subscribe(msgType, StringCRC("Sub1"), [&receivedValue](const Message& msg) {
		const int* data = msg.GetData<int>();
		if (data != nullptr)
		{
			receivedValue = *data;
		}
	});

	int payload = 42;
	bus.SendImmediate(msgType, StringCRC("Sender"), &payload, sizeof(payload));

	EXPECT_EQ(receivedValue, 42);
}

TEST(MessageBus, SendImmediateMultipleSubscribers)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");
	int callCount = 0;

	bus.Subscribe(msgType, StringCRC("Sub1"), [&callCount](const Message&) { callCount++; });
	bus.Subscribe(msgType, StringCRC("Sub2"), [&callCount](const Message&) { callCount++; });

	int payload = 1;
	bus.SendImmediate(msgType, StringCRC("Sender"), &payload, sizeof(payload));

	EXPECT_EQ(callCount, 2);
}

TEST(MessageBus, SendImmediateNoSubscribersIsNoop)
{
	MessageBus bus;
	int payload = 1;
	bus.SendImmediate(StringCRC("TestMsg"), StringCRC("Sender"), &payload, sizeof(payload));
}

TEST(MessageBus, PostMessageQueues)
{
	MessageBus bus;
	int payload = 1;

	bus.PostMessage(StringCRC("TestMsg"), StringCRC("Sender"), &payload, sizeof(payload));
	EXPECT_EQ(bus.GetQueuedMessageCount(), 1u);

	bus.PostMessage(StringCRC("TestMsg"), StringCRC("Sender"), &payload, sizeof(payload));
	EXPECT_EQ(bus.GetQueuedMessageCount(), 2u);
}

TEST(MessageBus, ProcessQueueDispatchesFIFO)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");
	std::vector<int> received;

	bus.Subscribe(msgType, StringCRC("Sub1"), [&received](const Message& msg) {
		const int* data = msg.GetData<int>();
		if (data != nullptr)
		{
			received.push_back(*data);
		}
	});

	int val1 = 10;
	int val2 = 20;
	bus.PostMessage(msgType, StringCRC("Sender"), &val1, sizeof(val1));
	bus.PostMessage(msgType, StringCRC("Sender"), &val2, sizeof(val2));

	EXPECT_EQ(received.size(), 0u);

	bus.ProcessQueue();

	ASSERT_EQ(received.size(), 2u);
	EXPECT_EQ(received[0], 10);
	EXPECT_EQ(received[1], 20);
}

TEST(MessageBus, ProcessQueueClearsQueue)
{
	MessageBus bus;
	int payload = 1;

	bus.PostMessage(StringCRC("TestMsg"), StringCRC("Sender"), &payload, sizeof(payload));
	EXPECT_EQ(bus.GetQueuedMessageCount(), 1u);

	bus.ProcessQueue();
	EXPECT_EQ(bus.GetQueuedMessageCount(), 0u);
}

TEST(MessageBus, TypeSafeDataAccess)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");

	struct TestPayload
	{
		int x;
		float y;
	};

	TestPayload receivedPayload = { 0, 0.0f };

	bus.Subscribe(msgType, StringCRC("Sub1"), [&receivedPayload](const Message& msg) {
		const TestPayload* data = msg.GetData<TestPayload>();
		if (data != nullptr)
		{
			receivedPayload = *data;
		}
	});

	TestPayload sendPayload = { 7, 3.14f };
	bus.SendImmediate(msgType, StringCRC("Sender"), sendPayload);

	EXPECT_EQ(receivedPayload.x, 7);
	EXPECT_FLOAT_EQ(receivedPayload.y, 3.14f);
}

TEST(MessageBus, GetDataWrongSizeReturnsNull)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");
	bool gotNull = false;

	bus.Subscribe(msgType, StringCRC("Sub1"), [&gotNull](const Message& msg) {
		const double* data = msg.GetData<double>();
		gotNull = (data == nullptr);
	});

	int payload = 42;
	bus.SendImmediate(msgType, StringCRC("Sender"), &payload, sizeof(payload));

	EXPECT_TRUE(gotNull);
}

TEST(MessageBus, ResubscribeUpdatesHandler)
{
	MessageBus bus;
	StringCRC msgType("TestMsg");
	StringCRC subId("Sub1");
	int handlerACalls = 0;
	int handlerBCalls = 0;

	bus.Subscribe(msgType, subId, [&handlerACalls](const Message&) { handlerACalls++; });
	bus.Subscribe(msgType, subId, [&handlerBCalls](const Message&) { handlerBCalls++; });

	EXPECT_EQ(bus.GetSubscriberCount(msgType), 1u);

	int payload = 1;
	bus.SendImmediate(msgType, StringCRC("Sender"), &payload, sizeof(payload));

	EXPECT_EQ(handlerACalls, 0);
	EXPECT_EQ(handlerBCalls, 1);
}
