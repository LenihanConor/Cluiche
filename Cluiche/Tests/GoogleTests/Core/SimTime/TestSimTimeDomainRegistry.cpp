// TestSimTimeDomainRegistry.cpp - Google Test unit tests for SimTimeDomainRegistry
//
// SimTimeDomainRegistry manages a named tree of SimTimeDomain instances. The
// world domain is externally owned (injected) and externally clocked, so
// TickAll() advances only the non-world sub-domains. Sub-domains compose their
// effective time scale from their ancestor chain and inherit ancestor pause.
//
// Scale composition is applied via SimTimeDomain::SetScale + Tick, so it
// inherits TimeServer's one-tick SetScale latency (a set scale takes effect
// starting the NEXT tick). Tests below tick once before asserting composed
// effect, mirroring TestSimTimeDomain.cpp's SetScale latency test.

#include <gtest/gtest.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/SimTime/SimTimeDomainRegistry.h>

using namespace Dia::Core;
using namespace Dia::SimTime;

namespace
{
    SimTimeDomain MakeWorld()
    {
        return SimTimeDomain(SimTimeDomainRegistry::kWorldId, 60.0f, TimeAbsolute::Zero());
    }
}

// ==============================================================================
// World binding
// ==============================================================================

TEST(SimTimeDomainRegistry, FindWorld_ReturnsExternalInstance)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    // The registry binds to the externally-owned world domain; it must not own
    // a second copy.
    EXPECT_EQ(registry.Find(SimTimeDomainRegistry::kWorldId), &world);
}

TEST(SimTimeDomainRegistry, FindMissing_ReturnsNull)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    EXPECT_EQ(registry.Find(StringCRC("nope")), nullptr);
}

// ==============================================================================
// Create / Find
// ==============================================================================

TEST(SimTimeDomainRegistry, Create_SubDomainFindableWithWorldGranularity)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& child = registry.Create(StringCRC("arena"));

    EXPECT_EQ(registry.Find(StringCRC("arena")), &child);
    EXPECT_EQ(child.GetId(), StringCRC("arena"));
    // Ticks at the world's granularity and starts at the world's current time.
    EXPECT_EQ(child.Step(), world.Step());
    EXPECT_EQ(child.Now(), world.Now());
    EXPECT_FALSE(child.IsPaused());
    EXPECT_FLOAT_EQ(child.GetScale(), 1.0f);
}

// ==============================================================================
// TickAll advances non-world domains and does NOT re-advance the world
// ==============================================================================

TEST(SimTimeDomainRegistry, TickAll_AdvancesNonWorldDomainsNotWorld)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& a = registry.Create(StringCRC("a"));
    SimTimeDomain& b = registry.Create(StringCRC("b"));

    registry.TickAll();

    // World is externally clocked — TickAll must leave it untouched.
    EXPECT_EQ(world.GetTick(), 0u);
    EXPECT_EQ(world.Now(), TimeAbsolute::Zero());

    // Non-world domains advanced exactly once.
    EXPECT_EQ(a.GetTick(), 1u);
    EXPECT_EQ(b.GetTick(), 1u);
    EXPECT_EQ(a.Now(), TimeAbsolute::Zero() + a.Step());
    EXPECT_EQ(b.Now(), TimeAbsolute::Zero() + b.Step());
}

// ==============================================================================
// Child inherits parent (world) pause
// ==============================================================================

TEST(SimTimeDomainRegistry, TickAll_ChildInheritsWorldPause)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& child = registry.Create(StringCRC("arena"));

    world.Pause();
    registry.TickAll();
    registry.TickAll();

    // Ancestor (world) paused -> child frozen, even though its own flag is clear.
    EXPECT_FALSE(child.IsPaused());
    EXPECT_EQ(child.GetTick(), 0u);
    EXPECT_EQ(child.Now(), TimeAbsolute::Zero());

    // Resume the world and the child advances again.
    world.Resume();
    registry.TickAll();
    EXPECT_EQ(child.GetTick(), 1u);
    EXPECT_EQ(child.Now(), TimeAbsolute::Zero() + child.Step());
}

TEST(SimTimeDomainRegistry, TickAll_GrandchildInheritsIntermediatePause)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& parent = registry.Create(StringCRC("parent"));
    SimTimeDomain& child  = registry.Create(StringCRC("child"), StringCRC("parent"));

    parent.Pause();
    registry.TickAll();

    // Intermediate ancestor paused -> grandchild frozen; parent frozen too
    // (its own pause flag stops its Tick internally).
    EXPECT_EQ(parent.GetTick(), 0u);
    EXPECT_EQ(child.GetTick(), 0u);
    EXPECT_FALSE(child.IsPaused());
}

// ==============================================================================
// Independent scale composes (one-tick SetScale latency applies)
// ==============================================================================

TEST(SimTimeDomainRegistry, TickAll_ChildScaleComposesWithWorldScale)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& child = registry.Create(StringCRC("arena"));
    child.SetScale(2.0f);

    // GetScale reports local intent immediately.
    EXPECT_FLOAT_EQ(child.GetScale(), 2.0f);

    registry.TickAll();   // tick 1: composed scale not yet applied (latency) -> 1x step
    registry.TickAll();   // tick 2: composed scale (1 * 2 = 2) now applied  -> 2x step

    const TimeRelative step = child.Step();
    EXPECT_EQ(child.Now(), TimeAbsolute::Zero() + step + (step * 2.0f));
    EXPECT_EQ(child.GetTick(), 2u);
    // Local intent preserved across TickAll passes.
    EXPECT_FLOAT_EQ(child.GetScale(), 2.0f);
}

TEST(SimTimeDomainRegistry, TickAll_WorldScaleComposesIntoChild)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& child = registry.Create(StringCRC("arena"));

    // Slow the whole world to half speed; child has no local scale (1.0).
    world.SetScale(0.5f);

    registry.TickAll();   // tick 1: latency -> child advances 1x step
    registry.TickAll();   // tick 2: composed (0.5 * 1) applied -> 0.5x step

    const TimeRelative step = child.Step();
    EXPECT_EQ(child.Now(), TimeAbsolute::Zero() + step + (step * 0.5f));
    EXPECT_EQ(child.GetTick(), 2u);
}

TEST(SimTimeDomainRegistry, TickAll_IndependentSubtreesComposeSeparately)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& fast = registry.Create(StringCRC("fast"));
    SimTimeDomain& slow = registry.Create(StringCRC("slow"));

    fast.SetScale(2.0f);
    slow.SetScale(0.5f);

    // Two ticks so the composed scales are applied (past the one-tick latency).
    registry.TickAll();
    registry.TickAll();

    const TimeRelative step = fast.Step();   // same granularity for both
    EXPECT_EQ(fast.Now(), TimeAbsolute::Zero() + step + (step * 2.0f));
    EXPECT_EQ(slow.Now(), TimeAbsolute::Zero() + step + (step * 0.5f));
}

TEST(SimTimeDomainRegistry, TickAll_GrandchildComposesFullChain)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& parent = registry.Create(StringCRC("parent"));
    SimTimeDomain& child  = registry.Create(StringCRC("child"), StringCRC("parent"));

    world.SetScale(2.0f);
    parent.SetScale(2.0f);
    child.SetScale(2.0f);

    registry.TickAll();   // tick 1: latency
    registry.TickAll();   // tick 2: composed applied

    // Child effective scale = world(2) * parent(2) * child(2) = 8.
    const TimeRelative step = child.Step();
    EXPECT_EQ(child.Now(), TimeAbsolute::Zero() + step + (step * 8.0f));
    // Local intent still reads back as the child's own 2.0.
    EXPECT_FLOAT_EQ(child.GetScale(), 2.0f);
}

// ==============================================================================
// Destroy
// ==============================================================================

TEST(SimTimeDomainRegistry, Destroy_RemovesDomainAndTickAllSkipsIt)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    registry.Create(StringCRC("a"));
    SimTimeDomain& b = registry.Create(StringCRC("b"));

    registry.Destroy(StringCRC("a"));
    EXPECT_EQ(registry.Find(StringCRC("a")), nullptr);

    // Surviving domain still advances; pointer stays valid across the destroy.
    registry.TickAll();
    EXPECT_EQ(b.GetTick(), 1u);
}

// Destroying an ancestor leaves its children with a dangling parentId.
// ComputeAncestorChain's walk stops at the first missing entry WITHOUT falling
// through to the world root — so a child whose parent was destroyed composes
// no ancestor scale at all (not even the world's), and is not treated as
// paused by an ancestor it can no longer see.
TEST(SimTimeDomainRegistry, Destroy_ParentLeavesChildWithNoAncestorComposition)
{
    SimTimeDomain world = MakeWorld();
    SimTimeDomainRegistry registry(world);

    SimTimeDomain& parent = registry.Create(StringCRC("parent"));
    SimTimeDomain& child  = registry.Create(StringCRC("child"), StringCRC("parent"));
    parent.SetScale(2.0f);
    world.SetScale(4.0f);
    world.Pause();   // even the world's pause must NOT reach the orphaned child

    registry.Destroy(StringCRC("parent"));
    EXPECT_EQ(registry.Find(StringCRC("parent")), nullptr);

    registry.TickAll();

    EXPECT_EQ(child.GetTick(), 1u)
        << "an orphaned child (dangling parentId) must still advance every TickAll pass";
    EXPECT_EQ(child.Now(), TimeAbsolute::Zero() + child.Step())
        << "with a dangling parent the walk stops before reaching the world root, "
           "so no ancestor scale composes in and the world's pause does not apply";
}
