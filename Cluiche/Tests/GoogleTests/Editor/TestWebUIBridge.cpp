#include <gtest/gtest.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;
using namespace Dia::Core;

TEST(WebUIBridge, ConstructsWithNullBridge)
{
    WebUIBridge bridge(nullptr);
    EXPECT_TRUE(true);
}

TEST(WebUIBridge, RegisterEventHandler)
{
    WebUIBridge bridge(nullptr);

    bool handlerCalled = false;
    bridge.RegisterEventHandler(StringCRC("test_event"), [&](const Json::Value&)
    {
        handlerCalled = true;
    });

    EXPECT_FALSE(handlerCalled);
}

TEST(WebUIBridge, MultipleHandlers)
{
    WebUIBridge bridge(nullptr);

    int callCount = 0;
    for (int i = 0; i < 5; ++i)
    {
        bridge.RegisterEventHandler(StringCRC("event"), [&](const Json::Value&) { ++callCount; });
    }

    EXPECT_EQ(callCount, 0);
}
