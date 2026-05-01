#include <gtest/gtest.h>

#include "DiaDebugServer/SubscriptionManager.h"

using namespace Dia::DebugServer;

class SubscriptionManagerTest : public ::testing::Test
{
protected:
	SubscriptionManager mgr;
};

TEST_F(SubscriptionManagerTest, InitiallyEmpty)
{
	EXPECT_EQ(mgr.GetSubscriptionCount(), 0);
}

TEST_F(SubscriptionManagerTest, Subscribe_IncreasesCount)
{
	mgr.Subscribe(1, Dia::Core::StringCRC("processing_unit_state"), Json::Value::null);
	EXPECT_EQ(mgr.GetSubscriptionCount(), 1);
}

TEST_F(SubscriptionManagerTest, Subscribe_IsSubscribedReturnsTrue)
{
	Dia::Core::StringCRC dataType("phase_transition");
	mgr.Subscribe(1, dataType, Json::Value::null);
	EXPECT_TRUE(mgr.IsSubscribed(1, dataType));
}

TEST_F(SubscriptionManagerTest, IsSubscribed_ReturnsFalseForUnknown)
{
	EXPECT_FALSE(mgr.IsSubscribed(1, Dia::Core::StringCRC("nonexistent")));
}

TEST_F(SubscriptionManagerTest, Subscribe_DuplicateUpdatesFilter)
{
	Dia::Core::StringCRC dataType("module_state");
	Json::Value filter1;
	filter1["pu_id"] = "MainPU";
	Json::Value filter2;
	filter2["pu_id"] = "SecondPU";

	mgr.Subscribe(1, dataType, filter1);
	EXPECT_EQ(mgr.GetSubscriptionCount(), 1);

	mgr.Subscribe(1, dataType, filter2);
	EXPECT_EQ(mgr.GetSubscriptionCount(), 1);
}

TEST_F(SubscriptionManagerTest, Subscribe_MultipleConnectionsSameType)
{
	Dia::Core::StringCRC dataType("phase_transition");
	mgr.Subscribe(1, dataType, Json::Value::null);
	mgr.Subscribe(2, dataType, Json::Value::null);
	mgr.Subscribe(3, dataType, Json::Value::null);

	EXPECT_EQ(mgr.GetSubscriptionCount(), 3);
	EXPECT_TRUE(mgr.IsSubscribed(1, dataType));
	EXPECT_TRUE(mgr.IsSubscribed(2, dataType));
	EXPECT_TRUE(mgr.IsSubscribed(3, dataType));
}

TEST_F(SubscriptionManagerTest, Subscribe_SameConnectionMultipleTypes)
{
	Dia::Core::StringCRC type1("phase_transition");
	Dia::Core::StringCRC type2("module_state");

	mgr.Subscribe(1, type1, Json::Value::null);
	mgr.Subscribe(1, type2, Json::Value::null);

	EXPECT_EQ(mgr.GetSubscriptionCount(), 2);
	EXPECT_TRUE(mgr.IsSubscribed(1, type1));
	EXPECT_TRUE(mgr.IsSubscribed(1, type2));
}

TEST_F(SubscriptionManagerTest, Unsubscribe_RemovesSubscription)
{
	Dia::Core::StringCRC dataType("phase_transition");
	mgr.Subscribe(1, dataType, Json::Value::null);
	EXPECT_TRUE(mgr.IsSubscribed(1, dataType));

	mgr.Unsubscribe(1, dataType);
	EXPECT_FALSE(mgr.IsSubscribed(1, dataType));
	EXPECT_EQ(mgr.GetSubscriptionCount(), 0);
}

TEST_F(SubscriptionManagerTest, Unsubscribe_NonexistentIsNoop)
{
	mgr.Unsubscribe(99, Dia::Core::StringCRC("nonexistent"));
	EXPECT_EQ(mgr.GetSubscriptionCount(), 0);
}

TEST_F(SubscriptionManagerTest, UnsubscribeAll_RemovesAllForConnection)
{
	Dia::Core::StringCRC type1("phase_transition");
	Dia::Core::StringCRC type2("module_state");
	Dia::Core::StringCRC type3("processing_unit_state");

	mgr.Subscribe(1, type1, Json::Value::null);
	mgr.Subscribe(1, type2, Json::Value::null);
	mgr.Subscribe(2, type3, Json::Value::null);

	EXPECT_EQ(mgr.GetSubscriptionCount(), 3);

	mgr.UnsubscribeAll(1);

	EXPECT_EQ(mgr.GetSubscriptionCount(), 1);
	EXPECT_FALSE(mgr.IsSubscribed(1, type1));
	EXPECT_FALSE(mgr.IsSubscribed(1, type2));
	EXPECT_TRUE(mgr.IsSubscribed(2, type3));
}

TEST_F(SubscriptionManagerTest, UnsubscribeAll_NonexistentConnectionIsNoop)
{
	mgr.Subscribe(1, Dia::Core::StringCRC("phase_transition"), Json::Value::null);
	mgr.UnsubscribeAll(99);
	EXPECT_EQ(mgr.GetSubscriptionCount(), 1);
}

TEST_F(SubscriptionManagerTest, GetSubscribers_ReturnsCorrectConnections)
{
	Dia::Core::StringCRC dataType("phase_transition");
	mgr.Subscribe(1, dataType, Json::Value::null);
	mgr.Subscribe(3, dataType, Json::Value::null);
	mgr.Subscribe(2, Dia::Core::StringCRC("module_state"), Json::Value::null);

	auto subscribers = mgr.GetSubscribers(dataType);
	EXPECT_EQ(subscribers.Size(), 2u);

	bool found1 = false, found3 = false;
	for (unsigned int i = 0; i < subscribers.Size(); ++i)
	{
		if (subscribers[i] == 1) found1 = true;
		if (subscribers[i] == 3) found3 = true;
	}
	EXPECT_TRUE(found1);
	EXPECT_TRUE(found3);
}

TEST_F(SubscriptionManagerTest, GetSubscribers_EmptyForNoSubscriptions)
{
	auto subscribers = mgr.GetSubscribers(Dia::Core::StringCRC("nonexistent"));
	EXPECT_EQ(subscribers.Size(), 0u);
}
