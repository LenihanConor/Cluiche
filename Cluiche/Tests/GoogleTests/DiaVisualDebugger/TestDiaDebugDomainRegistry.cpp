////////////////////////////////////////////////////////////////////////////////
// Filename: TestDiaDebugDomainRegistry.cpp
// Description: Unit tests for DiaDebugDomainRegistry — grouped domain registry
//              that replaces the flat DebugLayerManager array.
//              Tests cover C++ logic only.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/Json/external/json/json.h>

// =====================================================================
// Test helpers
// =====================================================================

namespace
{
    // Minimal IDebugDomain stub for testing.
    class StubDebugDomain : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        explicit StubDebugDomain(const char* id, const char* group)
            : mId(id)
            , mGroup(group)
        {}

        Dia::Core::StringCRC GetDomainId()     const override { return mId; }
        const char*          GetDisplayName()  const override { return mId.AsChar(); }
        const char*          GetDescription()  const override { return "stub"; }
        Dia::Core::StringCRC GetGroup()        const override { return mGroup; }
        Dia::Core::RGBA      GetAccentColour() const override { return Dia::Core::RGBA(0, 0, 0, 255); }

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Json::Value& out) override
        {
            out["drawers"] = Json::Value(Json::arrayValue);
            out["stats"]   = Json::Value(Json::objectValue);
        }

        void OnCommand(Dia::Core::StringCRC /*cmd*/, const Json::Value& /*args*/) override {}

    private:
        Dia::Core::StringCRC mId;
        Dia::Core::StringCRC mGroup;
    };
}

// =====================================================================
// Suite: DiaDebugDomainRegistry — basic registration
// =====================================================================

TEST(DiaDebugDomainRegistry, Register_FindDomain_ReturnsIt)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physics("physics", "Physics");

    registry.Register(physics);

    Dia::VisualDebugger::IDebugDomain* found = registry.FindDomain(Dia::Core::StringCRC("physics"));
    EXPECT_EQ(found, &physics);
}

TEST(DiaDebugDomainRegistry, FindDomain_UnknownId_ReturnsNullptr)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;

    Dia::VisualDebugger::IDebugDomain* found = registry.FindDomain(Dia::Core::StringCRC("does.not.exist"));
    EXPECT_EQ(found, nullptr);
}

TEST(DiaDebugDomainRegistry, Unregister_FindDomain_ReturnsNullptr)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physics("physics", "Physics");

    registry.Register(physics);
    registry.Unregister(physics);

    Dia::VisualDebugger::IDebugDomain* found = registry.FindDomain(Dia::Core::StringCRC("physics"));
    EXPECT_EQ(found, nullptr);
}

TEST(DiaDebugDomainRegistry, Unregister_NotRegistered_IsNoOp)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physics("physics", "Physics");

    // Should not crash
    EXPECT_NO_FATAL_FAILURE(registry.Unregister(physics));
}

// =====================================================================
// Suite: DiaDebugDomainRegistry — GetDomainCount
// =====================================================================

TEST(DiaDebugDomainRegistry, GetDomainCount_EmptyRegistry_IsZero)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    EXPECT_EQ(registry.GetDomainCount(), 0);
}

TEST(DiaDebugDomainRegistry, GetDomainCount_AfterRegister_MatchesCount)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain domA("domain.a", "Physics");
    StubDebugDomain domB("domain.b", "AI");
    StubDebugDomain domC("domain.c", "Physics");

    registry.Register(domA);
    EXPECT_EQ(registry.GetDomainCount(), 1);

    registry.Register(domB);
    EXPECT_EQ(registry.GetDomainCount(), 2);

    registry.Register(domC);
    EXPECT_EQ(registry.GetDomainCount(), 3);
}

TEST(DiaDebugDomainRegistry, GetDomainCount_AfterUnregister_Decrements)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain domA("domain.a", "Physics");
    StubDebugDomain domB("domain.b", "AI");

    registry.Register(domA);
    registry.Register(domB);
    EXPECT_EQ(registry.GetDomainCount(), 2);

    registry.Unregister(domA);
    EXPECT_EQ(registry.GetDomainCount(), 1);
}

// =====================================================================
// Suite: DiaDebugDomainRegistry — VisitGroup
// =====================================================================

TEST(DiaDebugDomainRegistry, VisitGroup_VisitsOnlyMatchingGroup)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physA("physics.shapes", "Physics");
    StubDebugDomain physB("physics.forces", "Physics");
    StubDebugDomain aiDom("ai.utility",    "AI");

    registry.Register(physA);
    registry.Register(physB);
    registry.Register(aiDom);

    int physicsVisited = 0;
    registry.VisitGroup(Dia::Core::StringCRC("Physics"), [&](Dia::VisualDebugger::IDebugDomain& /*d*/)
    {
        ++physicsVisited;
    });

    EXPECT_EQ(physicsVisited, 2);
}

TEST(DiaDebugDomainRegistry, VisitGroup_DoesNotVisitOtherGroups)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physA("physics.shapes", "Physics");
    StubDebugDomain aiDom("ai.utility",    "AI");

    registry.Register(physA);
    registry.Register(aiDom);

    int aiVisited = 0;
    registry.VisitGroup(Dia::Core::StringCRC("AI"), [&](Dia::VisualDebugger::IDebugDomain& d)
    {
        ++aiVisited;
        EXPECT_EQ(d.GetDomainId(), Dia::Core::StringCRC("ai.utility"));
    });

    EXPECT_EQ(aiVisited, 1);
}

TEST(DiaDebugDomainRegistry, VisitGroup_UnknownGroup_VisitsNothing)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physA("physics.shapes", "Physics");

    registry.Register(physA);

    int visited = 0;
    registry.VisitGroup(Dia::Core::StringCRC("Rendering"), [&](Dia::VisualDebugger::IDebugDomain& /*d*/)
    {
        ++visited;
    });

    EXPECT_EQ(visited, 0);
}

// =====================================================================
// Suite: DiaDebugDomainRegistry — VisitAll
// =====================================================================

TEST(DiaDebugDomainRegistry, VisitAll_VisitsAllRegisteredDomains)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain domA("domain.a", "Physics");
    StubDebugDomain domB("domain.b", "AI");
    StubDebugDomain domC("domain.c", "Rendering");

    registry.Register(domA);
    registry.Register(domB);
    registry.Register(domC);

    int visited = 0;
    registry.VisitAll([&](Dia::VisualDebugger::IDebugDomain& /*d*/)
    {
        ++visited;
    });

    EXPECT_EQ(visited, 3);
}

TEST(DiaDebugDomainRegistry, VisitAll_EmptyRegistry_VisitsNothing)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;

    int visited = 0;
    registry.VisitAll([&](Dia::VisualDebugger::IDebugDomain& /*d*/)
    {
        ++visited;
    });

    EXPECT_EQ(visited, 0);
}

TEST(DiaDebugDomainRegistry, VisitAll_AfterUnregister_DoesNotVisitRemoved)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain domA("domain.a", "Physics");
    StubDebugDomain domB("domain.b", "AI");

    registry.Register(domA);
    registry.Register(domB);
    registry.Unregister(domA);

    int visited = 0;
    registry.VisitAll([&](Dia::VisualDebugger::IDebugDomain& d)
    {
        ++visited;
        EXPECT_EQ(d.GetDomainId(), Dia::Core::StringCRC("domain.b"));
    });

    EXPECT_EQ(visited, 1);
}

// =====================================================================
// Suite: DiaDebugDomainRegistry — duplicate registration
// =====================================================================

TEST(DiaDebugDomainRegistry, Register_DuplicateId_IsNoOp)
{
    // Second Register with the same domainId must not increase the count.
    // (DIA_ASSERT fires in debug builds — tested via EXPECT_DEATH.)
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain domA("physics.shapes", "Physics");
    StubDebugDomain domADup("physics.shapes", "Physics");

    registry.Register(domA);
    EXPECT_DEATH({ registry.Register(domADup); }, "");

    // After the (death-trapped) duplicate, the registry should still hold
    // exactly one entry — but since EXPECT_DEATH runs in a subprocess we
    // verify count independently.
    EXPECT_EQ(registry.GetDomainCount(), 1);

    // The original pointer is preserved.
    EXPECT_EQ(registry.FindDomain(Dia::Core::StringCRC("physics.shapes")), &domA);
}

// =====================================================================
// Suite: DiaDebugDomainRegistry — cross-group isolation
// =====================================================================

TEST(DiaDebugDomainRegistry, VisitGroup_AfterUnregisterFromGroup_CorrectCount)
{
    Dia::VisualDebugger::DiaDebugDomainRegistry registry;
    StubDebugDomain physA("physics.shapes", "Physics");
    StubDebugDomain physB("physics.forces", "Physics");

    registry.Register(physA);
    registry.Register(physB);
    registry.Unregister(physA);

    int visited = 0;
    registry.VisitGroup(Dia::Core::StringCRC("Physics"), [&](Dia::VisualDebugger::IDebugDomain& /*d*/)
    {
        ++visited;
    });

    EXPECT_EQ(visited, 1);
}

#endif // DIA_DEBUG
