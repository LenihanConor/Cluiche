// Dia/DiaCaptureTest/ExpectationEvaluator.cpp
#include <DiaCaptureTest/ExpectationEvaluator.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace Dia { namespace CaptureTest {

// ---------------------------------------------------------------------------
// Minimal JSON helpers — no external dependencies
// ---------------------------------------------------------------------------

// Copy a JSON string value (inside double quotes) starting at src into dst (capacity cap).
// Returns a pointer to the character after the closing quote, or nullptr on failure.
static const char* CopyJsonString(const char* src, char* dst, int cap)
{
    if (!src || *src != '"') return nullptr;
    ++src;
    int i = 0;
    while (*src && *src != '"')
    {
        if (i < cap - 1)
            dst[i++] = *src;
        ++src;
    }
    dst[i] = '\0';
    if (*src == '"') return src + 1;
    return nullptr;
}

// Find the value of a JSON string field named key within buf.
// Writes result into dst (capacity cap). Returns true on success.
static bool FindJsonStringField(const char* buf, const char* key, char* dst, int cap)
{
    // Build search pattern: "key"
    char pattern[128];
    int plen = (int)std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    (void)plen;

    const char* p = std::strstr(buf, pattern);
    if (!p) return false;
    p += std::strlen(pattern);

    // Skip whitespace and colon
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;

    return CopyJsonString(p, dst, cap) != nullptr;
}

// Find the numeric value of a JSON field named key within buf.
// Returns true on success.
static bool FindJsonNumberField(const char* buf, const char* key, float* out)
{
    char pattern[128];
    int plen = (int)std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    (void)plen;

    const char* p = std::strstr(buf, pattern);
    if (!p) return false;
    p += std::strlen(pattern);

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;

    char* end = nullptr;
    *out = std::strtof(p, &end);
    return end != p;
}

// Find a JSON array of two numbers: [row, col]
static bool FindJsonRegionField(const char* buf, const char* key,
                                unsigned int* row, unsigned int* col)
{
    char pattern[128];
    int plen = (int)std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    (void)plen;

    const char* p = std::strstr(buf, pattern);
    if (!p) return false;
    p += std::strlen(pattern);

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '[') return false;
    ++p;

    char* end = nullptr;
    long r = std::strtol(p, &end, 10);
    if (end == p) return false;
    p = end;
    while (*p == ' ' || *p == ',') ++p;
    long c = std::strtol(p, &end, 10);
    if (end == p) return false;

    *row = (unsigned int)r;
    *col = (unsigned int)c;
    return true;
}

// ---------------------------------------------------------------------------
// Parse a single rule object starting at '{' in src.
// Returns pointer past closing '}', or nullptr on failure.
// ---------------------------------------------------------------------------
const char* ExpectationEvaluator::ParseRule(const char* p, LoadedRule* rule)
{
    // Find the closing '}' for this rule object (not nested objects)
    // Copy the object into a local buffer to reuse field-search helpers
    if (!p) return nullptr;
    while (*p && *p != '{') ++p;
    if (!*p) return nullptr;

    // Scan to matching '}'
    int depth = 0;
    const char* start = p;
    const char* q = p;
    while (*q)
    {
        if (*q == '{') ++depth;
        else if (*q == '}') { --depth; if (depth == 0) { ++q; break; } }
        ++q;
    }

    // Copy into temp buffer
    int len = (int)(q - start);
    if (len <= 0 || len > 2048) return nullptr;
    char tmp[2048];
    if (len >= (int)sizeof(tmp)) return nullptr;
    std::memcpy(tmp, start, (size_t)len);
    tmp[len] = '\0';

    // Extract fields
    if (!FindJsonStringField(tmp, "id",   rule->ruleId,    sizeof(rule->ruleId)))    return nullptr;
    if (!FindJsonStringField(tmp, "type", rule->type,      sizeof(rule->type)))      return nullptr;
    if (!FindJsonStringField(tmp, "op",   rule->op,        sizeof(rule->op)))        return nullptr;

    float val = 0.0f;
    if (!FindJsonNumberField(tmp, "value", &val)) return nullptr;
    rule->value = val;

    // region (optional for metric rules)
    rule->regionRow = 0;
    rule->regionCol = 0;
    FindJsonRegionField(tmp, "region", &rule->regionRow, &rule->regionCol);

    // metric key (optional)
    rule->metricKey[0] = '\0';
    FindJsonStringField(tmp, "key", rule->metricKey, sizeof(rule->metricKey));

    return q;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

bool ExpectationEvaluator::Load(const char* path)
{
    mRuleCount = 0;

    if (!path) return false;

    FILE* f = std::fopen(path, "rb");
    if (!f) return false;

    static constexpr int kBufSize = 8192;
    char buf[kBufSize];
    int bytesRead = (int)std::fread(buf, 1, kBufSize - 1, f);
    std::fclose(f);

    if (bytesRead <= 0) return false;
    buf[bytesRead] = '\0';

    // Find "rules" array
    const char* rulesKey = std::strstr(buf, "\"rules\"");
    if (!rulesKey) return false;

    // Advance to '['
    const char* p = std::strchr(rulesKey, '[');
    if (!p) return false;
    ++p;

    // Parse rule objects until ']'
    while (*p)
    {
        // Skip whitespace and commas
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') ++p;
        if (*p == ']') break;
        if (*p != '{') { ++p; continue; }

        if (mRuleCount >= kMaxRules) break;

        LoadedRule rule;
        const char* next = ParseRule(p, &rule);
        if (!next) break;

        mRules[mRuleCount++] = rule;
        p = next;
    }

    return mRuleCount > 0;
}

// ---------------------------------------------------------------------------
// Comparison helper
// ---------------------------------------------------------------------------
static bool Compare(float actual, const char* op, float expected)
{
    if (std::strcmp(op, ">")  == 0) return actual >  expected;
    if (std::strcmp(op, "<")  == 0) return actual <  expected;
    if (std::strcmp(op, ">=") == 0) return actual >= expected;
    if (std::strcmp(op, "<=") == 0) return actual <= expected;
    if (std::strcmp(op, "==") == 0) return actual == expected;
    if (std::strcmp(op, "near") == 0) return std::fabsf(actual - expected) <= 0.01f;
    return false;
}

// ---------------------------------------------------------------------------
// Brightness of a region cell
// ---------------------------------------------------------------------------
static float RegionBrightness(const Dia::Graphics::FrameCaptureResult& capture,
                               unsigned int row, unsigned int col)
{
    if (!capture.data) return 0.0f;
    if (capture.width == 0 || capture.height == 0) return 0.0f;

    // 4x4 grid: each cell covers cellW x cellH pixels
    unsigned int gridDim = 4;
    unsigned int cellW = capture.width  / gridDim;
    unsigned int cellH = capture.height / gridDim;

    if (cellW == 0) cellW = 1;
    if (cellH == 0) cellH = 1;

    unsigned int x0 = col * cellW;
    unsigned int y0 = row * cellH;
    unsigned int x1 = x0 + cellW;
    unsigned int y1 = y0 + cellH;
    if (x1 > capture.width)  x1 = capture.width;
    if (y1 > capture.height) y1 = capture.height;

    const unsigned char* pixels = static_cast<const unsigned char*>(capture.data);
    double sum = 0.0;
    unsigned int count = 0;

    for (unsigned int y = y0; y < y1; ++y)
    {
        for (unsigned int x = x0; x < x1; ++x)
        {
            unsigned int offset = y * capture.pitch + x * 4;
            unsigned int r = pixels[offset + 0];
            unsigned int g = pixels[offset + 1];
            unsigned int b = pixels[offset + 2];
            sum += (r + g + b) / 3.0 / 255.0;
            ++count;
        }
    }

    return count > 0 ? (float)(sum / count) : 0.0f;
}

// ---------------------------------------------------------------------------
// maxDelta of a region from FrameDiffResult
// ---------------------------------------------------------------------------
static float RegionMaxDelta(const FrameDiffResult& diff,
                             unsigned int row, unsigned int col)
{
    for (unsigned int i = 0; i < diff.regionCount; ++i)
    {
        if (diff.regions[i].row == row && diff.regions[i].col == col)
            return (float)diff.regions[i].maxDelta;
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Metric lookup from JSON string
// ---------------------------------------------------------------------------
static bool LookupMetric(const char* metricJson, const char* key, float* out)
{
    if (!metricJson || !key) return false;

    // Find "metrics" object first
    const char* metricsObj = std::strstr(metricJson, "\"metrics\"");
    const char* searchBase = metricsObj ? metricsObj : metricJson;

    return FindJsonNumberField(searchBase, key, out);
}

// ---------------------------------------------------------------------------
// Evaluate
// ---------------------------------------------------------------------------
EvaluationResult ExpectationEvaluator::Evaluate(
    const Dia::Graphics::FrameCaptureResult& capture,
    const FrameDiffResult& diff,
    const char* metricJson) const
{
    EvaluationResult result;
    result.allPass   = true;
    result.ruleCount = mRuleCount;

    bool captureReady = (capture.status == Dia::Graphics::FrameCaptureResult::Status::kReady
                         && capture.data != nullptr);

    for (unsigned int i = 0; i < mRuleCount; ++i)
    {
        const LoadedRule& rule = mRules[i];
        RuleResult& rr = result.results[i];

        rr.ruleId         = rule.ruleId;
        rr.op             = rule.op;
        rr.expectedValue  = rule.value;
        rr.actualValue    = 0.0f;
        rr.pass           = false;

        if (std::strcmp(rule.type, "brightness") == 0)
        {
            if (!captureReady)
            {
                rr.actualValue = 0.0f;
                rr.pass        = false;
            }
            else
            {
                rr.actualValue = RegionBrightness(capture, rule.regionRow, rule.regionCol);
                rr.pass        = Compare(rr.actualValue, rule.op, rule.value);
            }
        }
        else if (std::strcmp(rule.type, "diff_delta") == 0)
        {
            rr.actualValue = RegionMaxDelta(diff, rule.regionRow, rule.regionCol);
            rr.pass        = Compare(rr.actualValue, rule.op, rule.value);
        }
        else if (std::strcmp(rule.type, "metric") == 0)
        {
            float metricVal = 0.0f;
            if (!LookupMetric(metricJson, rule.metricKey, &metricVal))
            {
                rr.actualValue = 0.0f;
                rr.pass        = false;
            }
            else
            {
                rr.actualValue = metricVal;
                rr.pass        = Compare(rr.actualValue, rule.op, rule.value);
            }
        }
        else
        {
            // Unknown type — fail
            rr.pass = false;
        }

        if (!rr.pass)
            result.allPass = false;
    }

    return result;
}

} }
