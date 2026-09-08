// Dia/DiaCaptureTest/ExpectationEvaluator.h
#pragma once
#include <DiaGraphics/Interface/FrameCapture.h>
#include <DiaCaptureTest/FrameDiff.h>

namespace Dia { namespace CaptureTest {

struct RuleResult
{
    const char*  ruleId;
    bool         pass;
    float        actualValue;
    float        expectedValue;
    const char*  op; // ">", "<", ">=", "<=", "==", "near"
};

struct EvaluationResult
{
    bool         allPass;
    RuleResult   results[32];
    unsigned int ruleCount;
};

// ExpectationEvaluator loads a .expectations.json file and evaluates its rules
// against a captured frame and optional metrics JSON string.
//
// Lifetime note: EvaluationResult::results[i].ruleId and .op point into the
// evaluator's internal storage. The EvaluationResult is valid only as long as
// the ExpectationEvaluator that produced it has not been reloaded or destroyed.
class ExpectationEvaluator
{
public:
    // Load .expectations.json from path.
    // Returns false if the file cannot be opened or fails to parse.
    bool Load(const char* path);

    // Evaluate all loaded rules against the capture result and diff.
    // Metric rules (type "metric") require metricJson != nullptr; otherwise they fail.
    EvaluationResult Evaluate(const Dia::Graphics::FrameCaptureResult& capture,
                              const FrameDiffResult& diff,
                              const char* metricJson = nullptr) const;

private:
    struct LoadedRule
    {
        char         ruleId[64];
        char         type[32];   // "brightness", "diff_delta", "metric"
        unsigned int regionRow;
        unsigned int regionCol;
        char         metricKey[64];
        char         op[8];
        float        value;
    };

    static constexpr unsigned int kMaxRules = 32;
    LoadedRule   mRules[kMaxRules];
    unsigned int mRuleCount = 0;

    static const char* ParseRule(const char* p, LoadedRule* rule);
};

} }
