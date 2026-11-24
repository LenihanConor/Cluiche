// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (AC4 structure, AC7, AC8, AC9)
// ReachabilityValidator::Validate with test-double guarantees and profiles.
#include <gtest/gtest.h>
#include <DraoiHexWorldTerrain/ReachabilityValidator.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>
#include <DraoiHexWorldTerrain/Testing/TestProfiles.h>
#include <DraoiHexWorldTerrain/Testing/ValidationTestWorlds.h>

#include <string>
#include <vector>

using namespace Draoi::HexWorld;
using namespace Draoi::HexWorld::Testing;
using Dia::Core::StringCRC;

namespace
{

bool Contains(const EvidenceList<Diagnostic>& list, const char* needle)
{
    for (const Diagnostic& d : list) { if (std::string(d.text).find(needle) != std::string::npos) { return true; } }
    return false;
}

// Records how many times it ran, to prove all guarantees run and in order.
class OrderProbe : public IWorldGuarantee
{
public:
    OrderProbe(const char* id, bool satisfied, std::vector<std::string>& log) : mId(id), mSatisfied(satisfied), mLog(log) {}
    StringCRC GetId() const override { return StringCRC(mId.c_str()); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        mLog.push_back(mId);
        GuaranteeResult r;
        r.satisfied = mSatisfied;   // idCrc deliberately left unset: the validator stamps it
        return r;
    }
private:
    std::string mId;
    bool mSatisfied;
    std::vector<std::string>& mLog;
};

} // namespace

// ---- AC4 structure: overall pass/fail -------------------------------------------------------------------------------

TEST(ReachabilityValidator, AC4_AllGuaranteesSatisfiedPasses)
{
    const WorldStructure w = MakeAllLandWorld(5, 5);
    BlockWaterProfile p("P");
    FixedGuarantee g1("G1", true), g2("G2", true), g3("G3", true);
    ValidationParams params;
    params.AddProfile(&p); params.AddGuarantee(&g1); params.AddGuarantee(&g2); params.AddGuarantee(&g3);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_TRUE(r.passed);
    EXPECT_EQ(r.guarantees.Size(), 3u);
    EXPECT_TRUE(r.diagnostics.IsEmpty());
}

TEST(ReachabilityValidator, AC4_OneFailureFailsTheOverallResult)
{
    const WorldStructure w = MakeAllLandWorld(5, 5);
    BlockWaterProfile p("P");
    FixedGuarantee g1("G1", true), g2("G2", false), g3("G3", true);
    ValidationParams params;
    params.AddProfile(&p); params.AddGuarantee(&g1); params.AddGuarantee(&g2); params.AddGuarantee(&g3);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(r.Find(StringCRC("G1"))->satisfied);
    EXPECT_FALSE(r.Find(StringCRC("G2"))->satisfied);
    EXPECT_TRUE(r.Find(StringCRC("G3"))->satisfied);
}

TEST(ReachabilityValidator, AllGuaranteesRunInInjectionOrderEvenAfterAFailure)
{
    const WorldStructure w = MakeAllLandWorld(4, 4);
    BlockWaterProfile p("P");
    std::vector<std::string> log;
    OrderProbe a("A", false, log), b("B", true, log), c("C", false, log);
    ValidationParams params;
    params.AddProfile(&p); params.AddGuarantee(&a); params.AddGuarantee(&b); params.AddGuarantee(&c);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_EQ(log, (std::vector<std::string>{ "A", "B", "C" }));
    ASSERT_EQ(r.guarantees.Size(), 3u);
    EXPECT_EQ(r.guarantees[0].idCrc, StringCRC("A").Value());   // stamped from GetId()
    EXPECT_EQ(r.guarantees[1].idCrc, StringCRC("B").Value());
    EXPECT_EQ(r.guarantees[2].idCrc, StringCRC("C").Value());
    EXPECT_FALSE(r.passed);
}

TEST(ReachabilityValidator, NoGuaranteesWithAProfilePassesVacuously)
{
    const WorldStructure w = MakeAllLandWorld(3, 3);
    BlockWaterProfile p("P");
    ValidationParams params;
    params.AddProfile(&p);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_TRUE(r.passed);
    EXPECT_EQ(r.guarantees.Size(), 0u);
}

// ---- AC8: no profile ------------------------------------------------------------------------------------------------

TEST(ReachabilityValidator, AC8_EmptyProfilesFailWithADiagnostic)
{
    const WorldStructure w = MakeAllLandWorld(4, 4);
    FixedGuarantee pass("Pass", true);
    ValidationParams params;
    params.AddGuarantee(&pass);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(Contains(r.diagnostics, "no navigation profile was supplied"));
    ASSERT_EQ(r.guarantees.Size(), 1u);                 // guarantees still run independently
    EXPECT_TRUE(r.guarantees[0].satisfied);
}

TEST(ReachabilityValidator, AC8_EmptyProfilesMakeNavigationGuaranteesFail)
{
    const WorldStructure w = MakeAllLandWorld(4, 4);
    NavigationProbeGuarantee probe("Probe", Idx(w, 0, 0), Idx(w, 1, 0));
    ValidationParams params;
    params.AddGuarantee(&probe);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_FALSE(r.passed);
    EXPECT_FALSE(r.Find(StringCRC("Probe"))->satisfied);
}

// ---- AC9: injected guarantees ---------------------------------------------------------------------------------------

TEST(ReachabilityValidator, AC9_AppendedAlwaysFailGuaranteeFailsAndIsLast)
{
    const WorldStructure w = MakeAllLandWorld(4, 4);
    BlockWaterProfile p("P");
    FixedGuarantee g1("G1", true), g2("G2", true), g3("G3", true), extra("Extra.AlwaysFail", false);
    ValidationParams params;
    params.AddProfile(&p); params.AddGuarantee(&g1); params.AddGuarantee(&g2); params.AddGuarantee(&g3);
    params.AddGuarantee(&extra);
    const ValidationResult r = ReachabilityValidator::Validate(w, params);
    EXPECT_FALSE(r.passed);
    ASSERT_EQ(r.guarantees.Size(), 4u);
    EXPECT_EQ(r.guarantees[3].idCrc, StringCRC("Extra.AlwaysFail").Value());
    ASSERT_NE(r.Find(StringCRC("Extra.AlwaysFail")), nullptr);
    EXPECT_FALSE(r.Find(StringCRC("Extra.AlwaysFail"))->satisfied);
}

TEST(ReachabilityValidator, AC9_AppendedAlwaysPassGuaranteeLeavesPassed)
{
    const WorldStructure w = MakeAllLandWorld(4, 4);
    BlockWaterProfile p("P");
    FixedGuarantee g1("G1", true), extra("Extra.AlwaysPass", true);
    ValidationParams params;
    params.AddProfile(&p); params.AddGuarantee(&g1); params.AddGuarantee(&extra);
    EXPECT_TRUE(ReachabilityValidator::Validate(w, params).passed);
}

TEST(ReachabilityValidator, AC9_CustomGuaranteeSeesTheSameNavigationAnswers)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile allowing("Q");
    NavigationProbeGuarantee across("Across", Idx(w, 1, 2), Idx(w, 8, 2));
    NavigationProbeGuarantee within("Within", Idx(w, 0, 0), Idx(w, 3, 4));

    ValidationParams onlyBlocking;
    onlyBlocking.AddProfile(&blocking); onlyBlocking.AddGuarantee(&across); onlyBlocking.AddGuarantee(&within);
    const ValidationResult r1 = ReachabilityValidator::Validate(w, onlyBlocking);
    EXPECT_FALSE(r1.Find(StringCRC("Across"))->satisfied);
    EXPECT_TRUE(r1.Find(StringCRC("Within"))->satisfied);
    EXPECT_FALSE(r1.passed);

    ValidationParams both;
    both.AddProfile(&blocking); both.AddProfile(&allowing); both.AddGuarantee(&across); both.AddGuarantee(&within);
    const ValidationResult r2 = ReachabilityValidator::Validate(w, both);
    EXPECT_TRUE(r2.Find(StringCRC("Across"))->satisfied);
    EXPECT_TRUE(r2.passed);
}

// ---- AC7: deterministic ---------------------------------------------------------------------------------------------

TEST(ReachabilityValidator, AC7_SameWorldAndParamsGiveIdenticalResults)
{
    const WorldStructure w = MakeChannelWorld(12, 7, 5, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile allowing("Q");
    NavigationProbeGuarantee a("A", Idx(w, 1, 1), Idx(w, 10, 5));
    NavigationProbeGuarantee b("B", Idx(w, 0, 0), Idx(w, 4, 6));
    FixedGuarantee f("F", false);
    ValidationParams params;
    params.AddProfile(&blocking); params.AddProfile(&allowing);
    params.AddGuarantee(&a); params.AddGuarantee(&b); params.AddGuarantee(&f);
    const ValidationResult r1 = ReachabilityValidator::Validate(w, params);
    const ValidationResult r2 = ReachabilityValidator::Validate(w, params);
    EXPECT_EQ(r1, r2);
}
