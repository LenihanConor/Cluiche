////////////////////////////////////////////////////////////////////////////////
// Filename: TestProcessingUnitDomainRegistry.cpp
// GoogleTest suite — DiaSimTime Task 2.2: wire SimTimeDomainRegistry to the
//                    PU's world domain and expose it.
//
// Coverage (plan acceptance criteria for Task 2.2):
//   - GetDomainRegistry().Find(kWorldId) returns the SAME instance as
//     GetWorldDomain() (dependency-injection binding, not a copy).
//   - Pausing the world domain then Update()-ing with a large dt still does
//     not advance the world clock (mirrors SimPU_PauseThenResume_NoCatchUpBurst).
//   - TickAll() with zero sub-domains registered is a safe no-op and does not
//     advance the world clock — proves the registry structurally cannot
//     double-tick the world (Issue 2/5), even before DiaSimTimeModule exists
//     to call TickAll() for real (that wiring lands in Task 4.4).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/SimTime/SimTimeDomainRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::SimTime::SimTimeDomainRegistry;

namespace {
    constexpr float kFixed60 = 1.0f / 60.0f;   // fixed step for a 60Hz PU
} // anonymous namespace

// ---------------------------------------------------------------------------
// The registry binds to mWorldDomain by reference — Find(kWorldId) must
// return the exact same address as GetWorldDomain(), never a copy.
// ---------------------------------------------------------------------------
TEST(ProcessingUnitDomainRegistry, FindWorldId_ReturnsSameInstanceAsGetWorldDomain)
{
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false);

    Dia::SimTime::SimTimeDomain* found = pu.GetDomainRegistry().Find(SimTimeDomainRegistry::kWorldId);
    ASSERT_NE(found, nullptr)
        << "kWorldId must always resolve — the world domain always exists (ST-009)";
    EXPECT_EQ(found, &pu.GetWorldDomain())
        << "The registry must bind to the PU's existing world domain by reference, "
           "never construct or copy a second one";
}

// ---------------------------------------------------------------------------
// kSim: pausing the world domain then Update()-ing with a large dt must not
//       advance the world clock, regardless of the registry's presence.
// (Mirrors SimPU_PauseThenResume_NoCatchUpBurst in TestProcessingUnitAccumulator.cpp.)
// ---------------------------------------------------------------------------
TEST(ProcessingUnitDomainRegistry, PausedWorldDomain_LargeUpdateDoesNotAdvanceTick)
{
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false);

    pu.GetWorldDomain().Pause();
    const uint64_t before = pu.GetWorldDomain().GetTick();

    // A large real dt while paused must bank nothing and drain nothing.
    pu.Update(100.0f * kFixed60);

    EXPECT_EQ(pu.GetWorldDomain().GetTick(), before)
        << "A paused world domain must not advance its clock via Update(), "
           "however large the dt — wiring the registry must not change this";
}

// ---------------------------------------------------------------------------
// TickAll() with zero sub-domains registered must be a safe no-op: it must
// not touch the world domain's tick count. This proves the registry
// structurally cannot double-tick kWorldId (Issue 2/5) — TickAll() only ever
// advances NON-world sub-domains, never the externally-clocked world root.
// ---------------------------------------------------------------------------
TEST(ProcessingUnitDomainRegistry, TickAllWithNoSubDomains_DoesNotAdvanceWorldTick)
{
    ProcessingUnit pu(StringCRC("SimPU"), 60.0f, false);

    const uint64_t before = pu.GetWorldDomain().GetTick();

    pu.GetDomainRegistry().TickAll();

    EXPECT_EQ(pu.GetWorldDomain().GetTick(), before)
        << "TickAll() must never advance kWorldId — the world domain is externally "
           "clocked by ProcessingUnit::Update(), not by the registry";

    // Also true after a normal sim tick has already advanced the world once:
    // TickAll() still must not add a second, unrequested advance.
    pu.Update(kFixed60);
    const uint64_t afterOneRealTick = pu.GetWorldDomain().GetTick();
    ASSERT_EQ(afterOneRealTick - before, 1u)
        << "Sanity check: exactly one fixed step should have drained";

    pu.GetDomainRegistry().TickAll();
    EXPECT_EQ(pu.GetWorldDomain().GetTick(), afterOneRealTick)
        << "TickAll() must not double-advance kWorldId on top of the PU's own tick";
}
