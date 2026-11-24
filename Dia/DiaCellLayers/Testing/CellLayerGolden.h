#pragma once

// Test helper (Testing/ convention): per-layer hash goldens for cell data, with a readable diff report.
//
// A "case" (e.g. "archipelago/small/seed3") is a set of named float layers. GoldenStore::Check compares the case
// against a committed golden file (hash, count, min/max/mean per layer). On a mismatch it names every layer that
// moved; when the local baseline dump (written by the last update, kept under out/, never committed) still matches
// the golden it also reports the changed-cell count, the first changed cell and the largest delta. The actual data
// is written to <dumpDir>/<case>.actual.json so two dumps can be diffed outside the test.
//
// Updating: set DIA_UPDATE_GOLDEN=1 (see UpdateRequested()). Check then rewrites that case's golden entry and
// baseline dump instead of comparing. Do not commit exact goldens while tuning is still moving.
//
// Float cells are hashed and dumped by bit pattern (NaN payloads canonicalised), so the check is exact.

#include <DiaCore/Json/external/json/json.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace Dia::CellLayers::Testing
{
    struct GoldenLayerInput
    {
        const char*  name   = nullptr;
        const float* values = nullptr;
        uint32_t     count  = 0;
    };

    struct LayerSummary
    {
        uint64_t hash     = 0;
        uint32_t count    = 0;
        uint32_t nanCount = 0;
        double   min      = 0.0;   // over non-NaN cells; 0 when there are none
        double   max      = 0.0;
        double   mean     = 0.0;
    };

    enum class GoldenOutcome { Match, Mismatch, Missing, Updated, Error };

    struct GoldenResult
    {
        GoldenOutcome            outcome = GoldenOutcome::Match;
        std::vector<std::string> report;   // empty on Match / Updated
    };

    namespace Detail
    {
        constexpr uint32_t kCanonicalNaNBits = 0x7FC00000u;

        inline uint32_t CanonicalBits(float v)
        {
            uint32_t bits;
            std::memcpy(&bits, &v, sizeof(bits));
            return std::isnan(v) ? kCanonicalNaNBits : bits;
        }

        inline float FromBits(uint32_t bits)
        {
            float v;
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }

        inline std::string HexHash(uint64_t h)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "0x%016llX", static_cast<unsigned long long>(h));
            return buf;
        }

        inline std::string Num(double v)
        {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%.9g", v);
            return buf;
        }

        inline std::string SafeFileName(const std::string& caseId)
        {
            std::string s = caseId;
            for (char& c : s)
            {
                const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '.';
                if (!ok) { c = '_'; }
            }
            return s;
        }

        inline bool ReadJson(const std::filesystem::path& file, Json::Value& out, std::string& error)
        {
            std::ifstream in(file, std::ios::binary);
            if (!in) { error = "cannot open " + file.string(); return false; }
            Json::CharReaderBuilder builder;
            std::string errs;
            if (!Json::parseFromStream(builder, in, &out, &errs)) { error = file.string() + ": " + errs; return false; }
            return true;
        }

        inline bool WriteJson(const std::filesystem::path& file, const Json::Value& value, const char* indent, std::string& error)
        {
            std::error_code ec;
            if (file.has_parent_path()) { std::filesystem::create_directories(file.parent_path(), ec); }
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out) { error = "cannot write " + file.string(); return false; }
            Json::StreamWriterBuilder builder;
            builder["indentation"] = indent;
            out << Json::writeString(builder, value) << "\n";
            return static_cast<bool>(out);
        }

        inline Json::Value DumpOf(const std::vector<GoldenLayerInput>& layers)
        {
            Json::Value root(Json::objectValue);
            for (const GoldenLayerInput& l : layers)
            {
                Json::Value entry(Json::objectValue);
                Json::Value bits(Json::arrayValue);
                for (uint32_t i = 0; i < l.count; ++i) { bits.append(static_cast<Json::UInt>(CanonicalBits(l.values[i]))); }
                entry["count"] = static_cast<Json::UInt>(l.count);
                entry["bits"]  = bits;
                root[l.name]   = entry;
            }
            return root;
        }
    }

    // FNV-1a 64 over the canonical bit patterns, seeded with the count so a prefix never collides with its parent.
    inline uint64_t HashLayer(const float* values, uint32_t count)
    {
        uint64_t h = 0xCBF29CE484222325ull;
        auto mix = [&h](uint32_t word)
        {
            for (int b = 0; b < 4; ++b)
            {
                h ^= static_cast<uint64_t>((word >> (8 * b)) & 0xFFu);
                h *= 0x100000001B3ull;
            }
        };
        mix(count);
        for (uint32_t i = 0; i < count; ++i) { mix(Detail::CanonicalBits(values[i])); }
        return h;
    }

    inline LayerSummary Summarise(const float* values, uint32_t count)
    {
        LayerSummary s;
        s.hash  = HashLayer(values, count);
        s.count = count;
        double sum = 0.0;
        uint32_t counted = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (std::isnan(values[i])) { ++s.nanCount; continue; }
            const double v = values[i];
            if (counted == 0) { s.min = s.max = v; }
            else { if (v < s.min) { s.min = v; } if (v > s.max) { s.max = v; } }
            sum += v;
            ++counted;
        }
        s.mean = counted ? sum / counted : 0.0;
        return s;
    }

    // True when DIA_UPDATE_GOLDEN is set to anything other than empty or "0".
    inline bool UpdateRequested()
    {
        const char* v = std::getenv("DIA_UPDATE_GOLDEN");
        return v && v[0] != '\0' && !(v[0] == '0' && v[1] == '\0');
    }

    class GoldenStore
    {
    public:
        GoldenStore(std::filesystem::path goldenFile, std::filesystem::path dumpDir, bool update = UpdateRequested())
            : mGoldenFile(std::move(goldenFile)), mDumpDir(std::move(dumpDir)), mUpdate(update) {}

        GoldenResult Check(const std::string& caseId, const std::vector<GoldenLayerInput>& layers) const
        {
            GoldenResult result;
            const std::filesystem::path baselineDump = mDumpDir / (Detail::SafeFileName(caseId) + ".json");
            const std::filesystem::path actualDump   = mDumpDir / (Detail::SafeFileName(caseId) + ".actual.json");

            Json::Value golden(Json::objectValue);
            std::string error;
            const bool goldenExists = std::filesystem::exists(mGoldenFile);
            if (goldenExists && !Detail::ReadJson(mGoldenFile, golden, error))
            {
                result.outcome = GoldenOutcome::Error;
                result.report.push_back("golden file unreadable: " + error);
                return result;
            }
            if (!golden.isObject() || (goldenExists && !golden["cases"].isObject()))
            {
                result.outcome = GoldenOutcome::Error;
                result.report.push_back("golden file is not a dia.cell_layer_golden document: " + mGoldenFile.string());
                return result;
            }

            if (mUpdate)
            {
                golden["format"]  = "dia.cell_layer_golden";
                golden["version"] = 1;
                Json::Value entry(Json::objectValue);
                for (const GoldenLayerInput& l : layers) { entry[l.name] = SummaryToJson(Summarise(l.values, l.count)); }
                golden["cases"][caseId] = entry;
                if (!Detail::WriteJson(mGoldenFile, golden, "  ", error) ||
                    !Detail::WriteJson(baselineDump, Detail::DumpOf(layers), "", error))
                {
                    result.outcome = GoldenOutcome::Error;
                    result.report.push_back(error);
                    return result;
                }
                result.outcome = GoldenOutcome::Updated;
                return result;
            }

            if (!goldenExists || !golden["cases"].isMember(caseId))
            {
                result.outcome = GoldenOutcome::Missing;
                result.report.push_back("no golden for case '" + caseId + "' in " + mGoldenFile.string() +
                                        " (run with DIA_UPDATE_GOLDEN=1 to create it)");
                return result;
            }

            const Json::Value& expected = golden["cases"][caseId];
            Json::Value baseline;
            std::string ignored;
            const bool haveBaseline = std::filesystem::exists(baselineDump) && Detail::ReadJson(baselineDump, baseline, ignored);

            const std::string prefix = "case '" + caseId + "' ";
            for (const GoldenLayerInput& l : layers)
            {
                if (!expected.isMember(l.name))
                {
                    result.report.push_back(prefix + "layer '" + l.name + "' is new (not in the golden)");
                    continue;
                }
                const LayerSummary want = SummaryFromJson(expected[l.name]);
                const LayerSummary got  = Summarise(l.values, l.count);
                if (want.hash == got.hash && want.count == got.count) { continue; }
                result.report.push_back(DescribeChange(prefix, l, want, got, haveBaseline ? &baseline : nullptr));
            }
            for (const std::string& name : expected.getMemberNames())
            {
                bool found = false;
                for (const GoldenLayerInput& l : layers) { if (name == l.name) { found = true; break; } }
                if (!found) { result.report.push_back(prefix + "layer '" + name + "' is in the golden but missing from the actual data"); }
            }

            if (result.report.empty()) { result.outcome = GoldenOutcome::Match; return result; }
            result.outcome = GoldenOutcome::Mismatch;
            if (Detail::WriteJson(actualDump, Detail::DumpOf(layers), "", error))
            {
                result.report.push_back(prefix + "actual data written to " + actualDump.string());
            }
            return result;
        }

    private:
        static Json::Value SummaryToJson(const LayerSummary& s)
        {
            Json::Value v(Json::objectValue);
            v["hash"]     = Detail::HexHash(s.hash);
            v["count"]    = static_cast<Json::UInt>(s.count);
            v["nanCount"] = static_cast<Json::UInt>(s.nanCount);
            v["min"]      = s.min;
            v["max"]      = s.max;
            v["mean"]     = s.mean;
            return v;
        }

        static LayerSummary SummaryFromJson(const Json::Value& v)
        {
            LayerSummary s;
            s.hash     = std::strtoull(v["hash"].asString().c_str(), nullptr, 16);
            s.count    = v["count"].asUInt();
            s.nanCount = v["nanCount"].asUInt();
            s.min      = v["min"].asDouble();
            s.max      = v["max"].asDouble();
            s.mean     = v["mean"].asDouble();
            return s;
        }

        static std::string DescribeChange(const std::string& prefix, const GoldenLayerInput& l, const LayerSummary& want,
                                          const LayerSummary& got, const Json::Value* baseline)
        {
            std::string head = prefix + "layer '" + l.name + "': ";
            if (want.count != got.count)
            {
                return head + "cell count " + std::to_string(want.count) + " -> " + std::to_string(got.count);
            }
            // Cell-level diff only when the local baseline is the data the golden hash was taken from.
            if (baseline && baseline->isMember(l.name))
            {
                const Json::Value& bits = (*baseline)[l.name]["bits"];
                if (bits.isArray() && bits.size() == l.count)
                {
                    std::vector<float> old(l.count);
                    for (uint32_t i = 0; i < l.count; ++i) { old[i] = Detail::FromBits(bits[i].asUInt()); }
                    if (HashLayer(old.data(), l.count) == want.hash)
                    {
                        uint32_t changed = 0, first = 0;
                        double maxDelta = 0.0;
                        for (uint32_t i = 0; i < l.count; ++i)
                        {
                            if (Detail::CanonicalBits(old[i]) == Detail::CanonicalBits(l.values[i])) { continue; }
                            if (changed == 0) { first = i; }
                            ++changed;
                            const double d = std::fabs(static_cast<double>(l.values[i]) - static_cast<double>(old[i]));
                            if (!std::isnan(d) && d > maxDelta) { maxDelta = d; }
                        }
                        return head + std::to_string(changed) + " of " + std::to_string(l.count) + " cells changed, first cell " +
                               std::to_string(first) + " (" + Detail::Num(old[first]) + " -> " + Detail::Num(l.values[first]) +
                               "), max |delta| " + Detail::Num(maxDelta);
                    }
                }
            }
            return head + "hash " + Detail::HexHash(want.hash) + " -> " + Detail::HexHash(got.hash) +
                   ", min " + Detail::Num(want.min) + " -> " + Detail::Num(got.min) +
                   ", max " + Detail::Num(want.max) + " -> " + Detail::Num(got.max) +
                   ", mean " + Detail::Num(want.mean) + " -> " + Detail::Num(got.mean) +
                   " (no matching baseline dump for a cell-level diff)";
        }

        std::filesystem::path mGoldenFile;
        std::filesystem::path mDumpDir;
        bool                  mUpdate;
    };
}
