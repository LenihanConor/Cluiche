#include <gtest/gtest.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/Testing/CalloutTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::AICallout;
using namespace Dia::AICallout::Testing;
using namespace Dia::Core::Containers;

TEST(CalloutQueryTests, QueryReturnsMatchingCallout)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle handle = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f);

    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    AssertInQueryResults(results, handle);
}

TEST(CalloutQueryTests, QueryExcludesByKind)
{
    CalloutRegistry registry;
    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));

    QueryFilter filter{ Dia::Core::StringCRC("AttackPosition"), Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    EXPECT_EQ(results.Size(), 0u);
}

TEST(CalloutQueryTests, QueryExcludesByDistance)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    // Emit at (100,0) — distance 100 from origin
    CalloutHandle handle = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(100.0f, 0.0f));

    QueryFilter filterClose{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 50.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> resultsClose;
    registry.Query(filterClose, resultsClose);
    EXPECT_EQ(resultsClose.Size(), 0u);

    QueryFilter filterFar{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> resultsFar;
    registry.Query(filterFar, resultsFar);
    AssertInQueryResults(resultsFar, handle);
}

TEST(CalloutQueryTests, QueryExcludesClaimedCallouts)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle handle = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f));
    registry.Claim(handle, Dia::Core::StringCRC("EntityA"));

    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    AssertNotInQueryResults(results, handle);
}

TEST(CalloutQueryTests, QueryFactionAnyFilterMatchesAnyCallout)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");

    Callout callout{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, Dia::Core::StringCRC("RedTeam"), 10.0f, Json::Value() };
    CalloutHandle handle = registry.Emit(callout);

    // filter.faction = kZero means accept any faction
    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    AssertInQueryResults(results, handle);
}

TEST(CalloutQueryTests, QuerySpecificFactionMatchesCallout)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    Dia::Core::StringCRC faction("BlueTeam");

    Callout callout{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, faction, 10.0f, Json::Value() };
    CalloutHandle handle = registry.Emit(callout);

    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, faction };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    AssertInQueryResults(results, handle);
}

TEST(CalloutQueryTests, QuerySpecificFactionNoMatchDifferent)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");

    Callout callout{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, Dia::Core::StringCRC("RedTeam"), 10.0f, Json::Value() };
    registry.Emit(callout);

    // Filter asks for BlueTeam, callout is RedTeam — neither is kZero, no match
    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC("BlueTeam") };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    EXPECT_EQ(results.Size(), 0u);
}

TEST(CalloutQueryTests, QueryCalloutFactionAnyMatchesAnyFilter)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");

    // callout.faction = kZero means it accepts any querier
    Callout callout{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, Dia::Core::StringCRC::kZero, 10.0f, Json::Value() };
    CalloutHandle handle = registry.Emit(callout);

    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC("BlueTeam") };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    AssertInQueryResults(results, handle);
}

TEST(CalloutQueryTests, QueryAppendsToResults)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f));
    EmitTestCallout(registry, kind, Dia::Maths::Vector2D(10.0f, 0.0f));

    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    EXPECT_EQ(results.Size(), 2u);
}

TEST(CalloutQueryTests, QueryDoesNotClearResults)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kindA("HelpNeeded");
    Dia::Core::StringCRC kindB("Retreat");

    EmitTestCallout(registry, kindA, Dia::Maths::Vector2D(0.0f, 0.0f));
    EmitTestCallout(registry, kindA, Dia::Maths::Vector2D(10.0f, 0.0f));
    EmitTestCallout(registry, kindB, Dia::Maths::Vector2D(20.0f, 0.0f));

    DynamicArrayC<CalloutHandle, 16> results;

    QueryFilter filterA{ kindA, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    registry.Query(filterA, results);
    EXPECT_EQ(results.Size(), 2u);

    // Query does not clear — kindB result appends
    QueryFilter filterB{ kindB, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    registry.Query(filterB, results);
    EXPECT_EQ(results.Size(), 3u);
}
