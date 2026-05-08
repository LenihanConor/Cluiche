#include <gtest/gtest.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
	struct FakeServiceA
	{
		static const StringCRC kUniqueId;
		int value = 42;
	};
	const StringCRC FakeServiceA::kUniqueId("FakeServiceA");

	struct FakeServiceB
	{
		static const StringCRC kUniqueId;
		float data = 3.14f;
	};
	const StringCRC FakeServiceB::kUniqueId("FakeServiceB");
}

TEST(PluginServiceLocator, GetService_NotRegistered_ReturnsNullptr)
{
	PluginServiceLocator locator;
	EXPECT_EQ(locator.GetService<FakeServiceA>(), nullptr);
}

TEST(PluginServiceLocator, RegisterAndGet_ReturnsCorrectPointer)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;
	locator.RegisterService(&serviceA);

	FakeServiceA* result = locator.GetService<FakeServiceA>();
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result, &serviceA);
	EXPECT_EQ(result->value, 42);
}

TEST(PluginServiceLocator, GetService_WrongType_ReturnsNullptr)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;
	locator.RegisterService(&serviceA);

	EXPECT_EQ(locator.GetService<FakeServiceB>(), nullptr);
}

TEST(PluginServiceLocator, MultipleServices_EachResolvesCorrectly)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;
	FakeServiceB serviceB;
	locator.RegisterService(&serviceA);
	locator.RegisterService(&serviceB);

	EXPECT_EQ(locator.GetService<FakeServiceA>(), &serviceA);
	EXPECT_EQ(locator.GetService<FakeServiceB>(), &serviceB);
}

TEST(PluginServiceLocator, UnregisterService_RemovesIt)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;
	locator.RegisterService(&serviceA);

	locator.UnregisterService<FakeServiceA>();
	EXPECT_EQ(locator.GetService<FakeServiceA>(), nullptr);
}

TEST(PluginServiceLocator, UnregisterService_OtherServicesUnaffected)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;
	FakeServiceB serviceB;
	locator.RegisterService(&serviceA);
	locator.RegisterService(&serviceB);

	locator.UnregisterService<FakeServiceA>();
	EXPECT_EQ(locator.GetService<FakeServiceA>(), nullptr);
	EXPECT_EQ(locator.GetService<FakeServiceB>(), &serviceB);
}

TEST(PluginServiceLocator, HasService_ReturnsTrueWhenRegistered)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;

	EXPECT_FALSE(locator.HasService<FakeServiceA>());
	locator.RegisterService(&serviceA);
	EXPECT_TRUE(locator.HasService<FakeServiceA>());
}

TEST(PluginServiceLocator, UnregisterService_NotRegistered_DoesNothing)
{
	PluginServiceLocator locator;
	locator.UnregisterService<FakeServiceA>();
	EXPECT_EQ(locator.GetService<FakeServiceA>(), nullptr);
}

TEST(PluginServiceLocator, ReRegisterAfterUnregister_Works)
{
	PluginServiceLocator locator;
	FakeServiceA serviceA;
	FakeServiceA serviceA2;
	serviceA2.value = 99;

	locator.RegisterService(&serviceA);
	locator.UnregisterService<FakeServiceA>();
	locator.RegisterService(&serviceA2);

	FakeServiceA* result = locator.GetService<FakeServiceA>();
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result->value, 99);
}
