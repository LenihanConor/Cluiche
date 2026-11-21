// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Public Interfaces, Goal 4a)
// Evidence containers and result types of the Reachability Validator.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/ValidationTypes.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/TestProfiles.h>

#include <string>
#include <type_traits>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

TEST(ValidationTypes, SmallEvidenceElementsAreTriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<Diagnostic>);
    EXPECT_TRUE(std::is_trivially_copyable_v<CellPair>);
    EXPECT_TRUE(std::is_trivially_copyable_v<Measurement>);
}

TEST(EvidenceList, AddIndexIterateAndCompare)
{
    EvidenceList<int> a, b;
    EXPECT_EQ(a.Size(), 0u);
    EXPECT_TRUE(a.IsEmpty());
    a.Add(3); a.Add(5); a.Add(8);
    EXPECT_EQ(a.Size(), 3u);
    EXPECT_FALSE(a.IsEmpty());
    EXPECT_EQ(a[0], 3);
    EXPECT_EQ(a[2], 8);
    int sum = 0;
    for (int v : a) { sum += v; }
    EXPECT_EQ(sum, 16);
    EXPECT_EQ(a.Span().size(), 3u);
    EXPECT_NE(a, b);
    b.Add(3); b.Add(5);
    EXPECT_NE(a, b);          // different sizes are not equal
    b.Add(8);
    EXPECT_EQ(a, b);
    b.Clear();
    EXPECT_TRUE(b.IsEmpty());
}

TEST(GuaranteeResult, DiagnosticsFormatTruncateAndCap)
{
    GuaranteeResult r;
    r.AddDiagnostic("Hearth at (%d,%d) unreachable from any Nexus", 4, 7);
    ASSERT_EQ(r.diagnostics.Size(), 1u);
    EXPECT_STREQ(r.diagnostics[0].text, "Hearth at (4,7) unreachable from any Nexus");

    GuaranteeResult longMsg;
    const std::string big(500, 'x');
    longMsg.AddDiagnostic("%s", big.c_str());
    ASSERT_EQ(longMsg.diagnostics.Size(), 1u);
    EXPECT_EQ(std::string(longMsg.diagnostics[0].text).size(), sizeof(Diagnostic::text) - 1);   // truncated, terminated

    GuaranteeResult many;
    for (unsigned i = 0; i < kMaxDiagnostics + 5; ++i) { many.AddDiagnostic("d%u", i); }
    EXPECT_EQ(many.diagnostics.Size(), kMaxDiagnostics);
    EXPECT_EQ(many.droppedDiagnostics, 5u);
    EXPECT_STREQ(many.diagnostics[0].text, "d0");
}

TEST(ValidationResult, FindByIdReturnsInjectionOrderEntries)
{
    ValidationResult v;
    EXPECT_EQ(v.Find(StringCRC("A")), nullptr);
    GuaranteeResult a; a.idCrc = StringCRC("A").Value(); a.satisfied = true;
    GuaranteeResult b; b.idCrc = StringCRC("B").Value(); b.satisfied = false;
    v.guarantees.Add(a); v.guarantees.Add(b);
    ASSERT_NE(v.Find(StringCRC("A")), nullptr);
    EXPECT_TRUE(v.Find(StringCRC("A"))->satisfied);
    ASSERT_NE(v.Find(StringCRC("B")), nullptr);
    EXPECT_FALSE(v.Find(StringCRC("B"))->satisfied);
    EXPECT_EQ(v.Find(StringCRC("C")), nullptr);
    EXPECT_EQ(v.Find(StringCRC("B")), &v.guarantees[1]);
}

TEST(ValidationResult, EqualityCoversEvidence)
{
    GuaranteeResult a, b;
    a.idCrc = b.idCrc = 7; a.satisfied = b.satisfied = true;
    EXPECT_EQ(a, b);
    b.violatingCells.Add(3);
    EXPECT_NE(a, b);
    a.violatingCells.Add(3);
    EXPECT_EQ(a, b);
    ProofPath p; p.from = 1; p.to = 2; p.profileCrc = 9; p.cells.Add(1); p.cells.Add(2);
    a.proofPaths.Add(p);
    EXPECT_NE(a, b);
    b.proofPaths.Add(p);
    EXPECT_EQ(a, b);
    a.measurements.Add({ MeasurementKeys::PairDistance().Value(), 1, 2, -1, 4.0f });
    EXPECT_NE(a, b);
}

TEST(MeasurementKeys, AreDistinctAndStable)
{
    const uint32_t keys[] = { MeasurementKeys::PairDistance().Value(), MeasurementKeys::MinimumSpacing().Value(),
                              StringCRC("thronesOfType").Value(), StringCRC("ancientInteriorsCells").Value() };
    for (size_t i = 0; i < 4; ++i) { for (size_t j = i + 1; j < 4; ++j) { EXPECT_NE(keys[i], keys[j]); } }
    EXPECT_EQ(MeasurementKeys::PairDistance(), StringCRC("pairDistance"));
}

TEST(ValidationParams, AddRejectsNullAndStopsAtCapacity)
{
    ValidationParams p;
    EXPECT_FALSE(p.AddProfile(nullptr));
    EXPECT_FALSE(p.AddGuarantee(nullptr));
    BlockWaterProfile prof("P");
    for (unsigned i = 0; i < kMaxValidationProfiles; ++i) { EXPECT_TRUE(p.AddProfile(&prof)); }
    EXPECT_FALSE(p.AddProfile(&prof));
    EXPECT_EQ(p.profiles.Size(), kMaxValidationProfiles);

    FixedGuarantee g("G", true);
    for (unsigned i = 0; i < kMaxValidationGuarantees; ++i) { EXPECT_TRUE(p.AddGuarantee(&g)); }
    EXPECT_FALSE(p.AddGuarantee(&g));
    EXPECT_EQ(p.guarantees.Size(), kMaxValidationGuarantees);
}

TEST(TestDoubles, FixedGuaranteeAndProfilesReportTheirIds)
{
    FixedGuarantee f("Always.Fail", false);
    EXPECT_EQ(f.GetId(), StringCRC("Always.Fail"));
    WorldStructure w;
    w.Resize({ 3, 3 });
    class NullQuery : public INavigationQuery
    {
    public:
        bool IsReachable(uint32_t, uint32_t) const override { return false; }
        bool FindProofPath(uint32_t, uint32_t, ProofPath&) const override { return false; }
        bool FindProofPathToAny(uint32_t, std::span<const uint32_t>, ProofPath&) const override { return false; }
    } q;
    const GuaranteeResult r = f.Check({ w, q });
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.idCrc, StringCRC("Always.Fail").Value());
    EXPECT_EQ(r.diagnostics.Size(), 1u);
    FixedGuarantee ok("Always.Pass", true);
    EXPECT_TRUE(ok.Check({ w, q }).satisfied);

    BlockWaterProfile bw("P");
    AllowWaterProfile aw("Q");
    EXPECT_EQ(bw.GetId(), StringCRC("P"));
    EXPECT_EQ(aw.GetId(), StringCRC("Q"));
    auto costs = bw.CreateCostProvider(w);
    ASSERT_NE(costs, nullptr);
    EXPECT_LT(costs->GetCost({ 0, 0 }, { 1, 0 }), 0.0f);   // default cells are DeepWater: blocked
    EXPECT_GE(aw.CreateCostProvider(w)->GetCost({ 0, 0 }, { 1, 0 }), 0.0f);
}
