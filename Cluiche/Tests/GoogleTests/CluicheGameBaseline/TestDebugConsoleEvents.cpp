#include <gtest/gtest.h>

#include <Types/DebugConsoleLineEvent.h>
#include <Types/DebugConsoleResultEvent.h>

#include <cstring>
#include <type_traits>

using Cluiche::AppFlow::DebugConsoleLineEvent;
using Cluiche::AppFlow::DebugConsoleResultEvent;

static_assert(std::is_trivially_copyable<DebugConsoleLineEvent>::value, "line event must be memcpy-able");
static_assert(std::is_trivially_copyable<DebugConsoleResultEvent>::value, "result event must be memcpy-able");

TEST(DebugConsoleEvents, LineEventRoundTripsThroughMemcpy)
{
    DebugConsoleLineEvent in{};
    in.seq = 42;
    std::strcpy(in.text, "debug.layer.enable x");

    DebugConsoleLineEvent out{};
    std::memcpy(&out, &in, sizeof(in));

    EXPECT_EQ(out.seq, 42u);
    EXPECT_STREQ(out.text, "debug.layer.enable x");
    EXPECT_EQ(sizeof(out.text), 256u);
}

TEST(DebugConsoleEvents, ResultEventRoundTripsThroughMemcpy)
{
    DebugConsoleResultEvent in{};
    in.seq = 7;
    in.exitCode = -3;
    in.parseError = 2;
    std::strcpy(in.text, "echo me");

    DebugConsoleResultEvent out{};
    std::memcpy(&out, &in, sizeof(in));

    EXPECT_EQ(out.seq, 7u);
    EXPECT_EQ(out.exitCode, -3);
    EXPECT_EQ(out.parseError, 2);
    EXPECT_STREQ(out.text, "echo me");
    EXPECT_EQ(sizeof(out.text), 256u);
}

TEST(DebugConsoleEvents, DefaultInitialisedIsZeroed)
{
    DebugConsoleLineEvent line{};
    DebugConsoleResultEvent res{};
    EXPECT_EQ(line.seq, 0u);
    EXPECT_EQ(line.text[0], '\0');
    EXPECT_EQ(res.exitCode, 0);
    EXPECT_EQ(res.parseError, 0);
    EXPECT_EQ(res.text[0], '\0');
}
