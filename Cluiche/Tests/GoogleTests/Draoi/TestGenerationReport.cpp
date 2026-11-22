// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/retry-mechanism.md (Generation Report, Public Interfaces)
// Report types of the retry mechanism: GenParams, GenerationOutcome, AttemptRecord, GenerationReport.
#include <gtest/gtest.h>
#include <DraoiHexWorldTerrain/GenerationReport.h>

#include <cstring>
#include <type_traits>

using namespace Draoi::HexWorld;

TEST(GenerationReport, AttemptRecordIsTriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<AttemptRecord>);
    EXPECT_TRUE(std::is_trivially_copyable_v<GenParams>);
}

TEST(GenerationReport, DefaultsAreEmpty)
{
    GenerationReport r;
    EXPECT_EQ(r.outcome, GenerationOutcome::Success);
    EXPECT_EQ(r.attempts.Size(), 0u);
    EXPECT_EQ(r.worldRevision, 0u);
    EXPECT_EQ(r.outerSeed, 0u);
    GenParams p;
    EXPECT_EQ(p.maxRetryAttempts, 10u);
}

TEST(GenerationReport, OutcomeNames)
{
    EXPECT_STREQ(GenerationOutcomeName(GenerationOutcome::Success), "Success");
    EXPECT_STREQ(GenerationOutcomeName(GenerationOutcome::Unsatisfiable), "Unsatisfiable");
    EXPECT_STREQ(GenerationOutcomeName(GenerationOutcome::Timeout), "Timeout");
    EXPECT_STREQ(GenerationOutcomeName(GenerationOutcome::Cancelled), "Cancelled");
    EXPECT_STREQ(GenerationOutcomeName(GenerationOutcome::Error), "Error");
}

TEST(GenerationReport, ClampMaxRetryAttempts)
{
    EXPECT_EQ(ClampMaxRetryAttempts(0), 1u);
    EXPECT_EQ(ClampMaxRetryAttempts(1), 1u);
    EXPECT_EQ(ClampMaxRetryAttempts(10), 10u);
    EXPECT_EQ(ClampMaxRetryAttempts(kMaxRetryAttemptsLimit), kMaxRetryAttemptsLimit);
    EXPECT_EQ(ClampMaxRetryAttempts(kMaxRetryAttemptsLimit + 1), kMaxRetryAttemptsLimit);
    EXPECT_EQ(ClampMaxRetryAttempts(0xFFFFFFFFu), kMaxRetryAttemptsLimit);
}

TEST(GenerationReport, AddFailedGuaranteeCapsAtTheLimit)
{
    AttemptRecord a;
    for (unsigned i = 0; i < kMaxValidationGuarantees + 5; ++i) { a.AddFailedGuarantee(1000 + i); }
    EXPECT_EQ(a.failedGuaranteeCount, kMaxValidationGuarantees);
    EXPECT_EQ(a.failedGuarantees[0], 1000u);
    EXPECT_EQ(a.failedGuarantees[kMaxValidationGuarantees - 1], 1000u + kMaxValidationGuarantees - 1);
}

TEST(GenerationReport, AddDiagnosticCapsAndCountsTheRest)
{
    AttemptRecord a;
    for (unsigned i = 0; i < kMaxAttemptDiagnostics + 3; ++i) { a.AddDiagnostic("diag %u", i); }
    EXPECT_EQ(a.diagnosticCount, kMaxAttemptDiagnostics);
    EXPECT_EQ(a.droppedDiagnostics, 3u);
    EXPECT_STREQ(a.diagnostics[0].text, "diag 0");
}

TEST(GenerationReport, SetPlaceFailureTruncatesAndTerminates)
{
    AttemptRecord a;
    char longText[300];
    std::memset(longText, 'x', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    a.SetPlaceFailure(PlaceResult::Code::Retryable, longText);
    EXPECT_EQ(a.placeCode, PlaceResult::Code::Retryable);
    EXPECT_EQ(std::strlen(a.placeReason), sizeof(a.placeReason) - 1);
}

TEST(GenerationReport, SameApartFromTimingIgnoresOnlyElapsedMs)
{
    GenerationReport a, b;
    a.outerSeed = b.outerSeed = 7;
    AttemptRecord r; r.attempt = 0; r.subSeed = 99; r.passed = false; r.AddFailedGuarantee(5); r.AddDiagnostic("boom");
    r.elapsedMs = 1.0f;
    a.attempts.Add(r);
    r.elapsedMs = 9.0f;
    b.attempts.Add(r);
    EXPECT_TRUE(SameApartFromTiming(a, b));

    b.attempts[0].subSeed = 100;
    EXPECT_FALSE(SameApartFromTiming(a, b));
    b.attempts[0].subSeed = 99;
    b.attempts[0].AddDiagnostic("more");
    EXPECT_FALSE(SameApartFromTiming(a, b));
}

TEST(GenerationReport, SameApartFromTimingComparesTopLevelFields)
{
    GenerationReport a, b;
    EXPECT_TRUE(SameApartFromTiming(a, b));
    b.outcome = GenerationOutcome::Unsatisfiable;
    EXPECT_FALSE(SameApartFromTiming(a, b));
    b = a; b.worldRevision = 3;
    EXPECT_FALSE(SameApartFromTiming(a, b));
    b = a; b.outerSeed = 1;
    EXPECT_FALSE(SameApartFromTiming(a, b));
    b = a; b.maxAttempts = 4;
    EXPECT_FALSE(SameApartFromTiming(a, b));
    b = a; b.finalValidation.passed = true;
    EXPECT_FALSE(SameApartFromTiming(a, b));
}
