#include <Shared/World/StratumTable.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

namespace CoW {
namespace World {

namespace
{
    // Plain struct bound to one sidecar key (ParamBinding requirement: standard layout).
    struct ShareParam
    {
        float share = 0.0f;
    };
    struct DepthParam
    {
        float depth = 0.0f;
    };

    // Shares of a set must complete 1 within this tolerance (float sums).
    constexpr double kShareSumTolerance = 1e-3;

    bool Fail(StratumError& err, const std::string& field, const std::string& reason)
    {
        err.field = field;
        err.reason = reason;
        DIA_LOG_ERROR("CoW", "StratumTable: load failed at '%s': %s", field.c_str(), reason.c_str());
        return false;
    }

    bool CopyName(char (&dst)[32], const std::string& name)
    {
        if (name.empty() || name.size() > StratumTable::kMaxNameLength) { return false; }
        std::memcpy(dst, name.c_str(), name.size() + 1);
        return true;
    }

    bool ReadInt(const Json::Value& v, int lo, int hi, int& out)
    {
        if (!v.isIntegral() || v.isBool()) { return false; }
        const Json::Int64 n = v.asInt64();
        if (n < lo || n > hi) { return false; }
        out = static_cast<int>(n);
        return true;
    }
}

bool StratumTable::LoadFromText(const char* strataText, const char* sidecarText, StratumError& err, const AssignerTypeCheck& knownAssignerType)
{
    err = StratumError();
    if (strataText == nullptr) { return Fail(err, "document", "no strata text"); }
    if (sidecarText == nullptr) { return Fail(err, "sidecar", "no strata sidecar text"); }

    Json::Value root;
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        builder["rejectDupKeys"] = true;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        bool ok = false;
        try { ok = reader->parse(strataText, strataText + std::strlen(strataText), &root, &errors); }
        catch (const std::exception&) { ok = false; }
        if (!ok) { return Fail(err, "document", "not valid JSON"); }
    }
    if (!root.isObject() || !root["format"].isString() || root["format"].asString() != "cow.strata")
    {
        return Fail(err, "format", "format must be \"cow.strata\"");
    }
    if (!root["version"].isUInt() || root["version"].asUInt() != 1) { return Fail(err, "version", "unsupported version"); }
    const Json::Value& sets = root["sets"];
    if (!sets.isArray()) { return Fail(err, "sets", "sets must be an array"); }
    if (sets.empty()) { return Fail(err, "sets", "at least one set is required"); }
    if (sets.size() > kMaxSets) { return Fail(err, "sets", "more sets than the table supports"); }

    StratumTable cand;
    for (Json::ArrayIndex si = 0; si < sets.size(); ++si)
    {
        const Json::Value& js = sets[si];
        if (!js.isObject() || !js["id"].isString()) { return Fail(err, "sets", "each set needs a string id"); }
        StratumSetSpec& set = cand.mSets[si];
        const std::string id = js["id"].asString();
        if (!CopyName(set.id, id)) { return Fail(err, "sets", "set id is empty or too long"); }
        const std::string where = "sets." + id;
        for (uint32_t k = 0; k < si; ++k)
        {
            if (std::strcmp(cand.mSets[k].id, set.id) == 0) { return Fail(err, where, "duplicate set id '" + id + "'"); }
        }

        if (js.isMember("ordered"))
        {
            if (!js["ordered"].isBool()) { return Fail(err, where + ".ordered", "set '" + id + "': ordered must be a boolean"); }
            set.ordered = js["ordered"].asBool();
        }

        const Json::Value& assigner = js["assigner"];
        if (!assigner.isObject() || !assigner["type"].isString())
        {
            return Fail(err, where + ".assigner", "set '" + id + "': assigner needs a string type");
        }
        const std::string type = assigner["type"].asString();
        const bool known = knownAssignerType ? knownAssignerType(type.c_str()) : type == kBuiltinAssignerType;
        if (!known || !CopyName(set.assignerType, type))
        {
            return Fail(err, where + ".assigner", "set '" + id + "': unknown assigner type '" + type + "'");
        }
        if (assigner.isMember("metric"))
        {
            // The set of known metrics is checked when metrics arrive (Strata task 4b).
            if (!assigner["metric"].isString() || assigner["metric"].asString().empty() || assigner["metric"].asString().size() > kMaxNameLength)
            {
                return Fail(err, where + ".assigner", "set '" + id + "': metric must be a non-empty string");
            }
            std::memcpy(set.metric, assigner["metric"].asCString(), assigner["metric"].asString().size() + 1);
        }

        const Json::Value& strata = js["strata"];
        if (!strata.isArray()) { return Fail(err, where + ".strata", "set '" + id + "': strata must be an array"); }
        if (strata.empty()) { return Fail(err, where + ".strata", "set '" + id + "': at least one stratum is required"); }
        if (strata.size() > kMaxStrata)
        {
            return Fail(err, where + ".strata", "set '" + id + "': more than " + std::to_string(kMaxStrata) + " strata");
        }

        for (Json::ArrayIndex i = 0; i < strata.size(); ++i)
        {
            const Json::Value& jz = strata[i];
            if (!jz.isObject() || !jz["name"].isString())
            {
                return Fail(err, where + ".strata", "set '" + id + "': each stratum needs a string name");
            }
            const std::string name = jz["name"].asString();
            if (name.empty() || name.size() > kMaxNameLength)
            {
                return Fail(err, where + ".strata", "set '" + id + "': stratum name is empty or too long");
            }
            const std::string zwhere = where + "." + name;
            if (name == "None") { return Fail(err, zwhere, "set '" + id + "': stratum name None is reserved for ocean"); }
            for (uint32_t k = 0; k < i; ++k)
            {
                if (name == set.strata[k].name) { return Fail(err, zwhere, "set '" + id + "': duplicate stratum name '" + name + "'"); }
            }
            StratumSpec& z = set.strata[i];
            CopyName(z.name, name);

            if (jz.isMember("share"))
            {
                // The sidecar owns the effective share; the file value is the authored default and only type-checked.
                if (!jz["share"].isNumeric() || jz["share"].isBool() || jz["share"].asDouble() < 0.0 || jz["share"].asDouble() > 1.0)
                {
                    return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': share must be a number in 0..1");
                }
            }
            int rmin = 0;
            int rmax = 0;
            if (!ReadInt(jz["regions_min"], 0, 255, rmin) || !ReadInt(jz["regions_max"], 0, 255, rmax))
            {
                return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': regions_min and regions_max must be integers 0..255");
            }
            if (rmin > rmax)
            {
                return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': regions_min is greater than regions_max");
            }
            z.regionsMin = static_cast<uint8_t>(rmin);
            z.regionsMax = static_cast<uint8_t>(rmax);
            if (jz.isMember("main_landmass_only"))
            {
                if (!jz["main_landmass_only"].isBool())
                {
                    return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': main_landmass_only must be a boolean");
                }
                z.mainLandmassOnly = jz["main_landmass_only"].asBool();
            }
            if (jz.isMember("min_coast_depth"))
            {
                int depth = 0;
                if (!ReadInt(jz["min_coast_depth"], 0, 1 << 20, depth))
                {
                    return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': min_coast_depth must be an integer >= 0");
                }
                z.minCoastDepth = depth;
                z.declaresMinCoastDepth = true;
            }
        }
        set.count = strata.size();
        ++cand.mSetCount;
    }

    for (uint32_t s = 0; s < cand.mSetCount; ++s)
    {
        if (std::strcmp(cand.mSets[s].id, kDepthSetId) == 0) { cand.mDepthIndex = static_cast<int>(s); }
    }
    if (cand.mDepthIndex < 0) { return Fail(err, "sets", std::string("no set with id '") + kDepthSetId + "' (the generator's set)"); }

    // Sidecar: one share parameter per stratum, keyed "<set id>.<stratum name>.share".
    Dia::Attribute::ParameterFile file;
    Dia::Attribute::ParamError perr;
    if (!file.Load(sidecarText, "strata.schema.json", nullptr, perr))
    {
        return Fail(err, "sidecar", perr.reason.AsCStr());
    }
    uint32_t expectedParams = 0;
    auto readFloat = [&](const std::string& key, float& value) -> bool
    {
        Dia::Attribute::ParamBinding<DepthParam> binding;
        binding.Bind(Dia::Core::StringCRC(key.c_str()), &DepthParam::depth);
        auto report = std::make_unique<Dia::Attribute::BindingReport>();
        binding.Resolve(file.Schema(), *report);
        if (report->bindingsWithoutEntry.Size() != 0 || report->kindMismatches.Size() != 0) { return false; }
        DepthParam p;
        binding.Snapshot(file.Values(), p);
        value = p.depth;
        return true;
    };
    for (uint32_t s = 0; s < cand.mSetCount; ++s)
    {
        StratumSetSpec& set = cand.mSets[s];
        expectedParams += set.count;
        double sum = 0.0;
        for (uint32_t i = 0; i < set.count; ++i)
        {
            const std::string key = std::string(set.id) + "." + set.strata[i].name + ".share";
            Dia::Attribute::ParamBinding<ShareParam> binding;
            binding.Bind(Dia::Core::StringCRC(key.c_str()), &ShareParam::share);
            auto report = std::make_unique<Dia::Attribute::BindingReport>();
            // One key per binding, so the schema's other entries are expected to be "unbound" here; only a missing or
            // wrongly typed entry for this key is a fault. Stray schema entries are caught by the count check below.
            binding.Resolve(file.Schema(), *report);
            if (report->bindingsWithoutEntry.Size() != 0 || report->kindMismatches.Size() != 0)
            {
                return Fail(err, "sidecar", "sidecar has no usable parameter '" + key + "'");
            }
            ShareParam p;
            binding.Snapshot(file.Values(), p);
            set.strata[i].share = p.share;
            sum += static_cast<double>(p.share);
            if (set.strata[i].declaresMinCoastDepth)
            {
                const std::string depthKey = std::string(set.id) + "." + set.strata[i].name + ".min_coast_depth";
                float depth = 0.0f;
                if (!readFloat(depthKey, depth))
                {
                    return Fail(err, "sidecar", "sidecar has no usable parameter '" + depthKey + "'");
                }
                set.strata[i].minCoastDepth = static_cast<int32_t>(std::lround(depth));
                ++expectedParams;
            }
        }
        if (std::fabs(sum - 1.0) > kShareSumTolerance)
        {
            return Fail(err, std::string("sets.") + set.id, std::string("set '") + set.id + "': stratum shares must complete 1 (sum is " + std::to_string(sum) + ")");
        }
    }

    if (file.Schema().GetAttributeCount() != expectedParams)
    {
        return Fail(err, "sidecar", "sidecar declares " + std::to_string(file.Schema().GetAttributeCount()) +
                                    " parameters but the strata file needs exactly " + std::to_string(expectedParams));
    }

    *this = cand;
    DIA_LOG_INFO("CoW", "StratumTable: loaded %u sets, depth set has %u strata", cand.mSetCount, cand.Count());
    return true;
}

const StratumSetSpec* StratumTable::FindSet(const char* id) const
{
    if (id == nullptr) { return nullptr; }
    for (uint32_t i = 0; i < mSetCount; ++i)
    {
        if (std::strcmp(mSets[i].id, id) == 0) { return &mSets[i]; }
    }
    return nullptr;
}

const char* StratumTable::Name(WorldGeneration::StratumIndex z) const
{
    if (z == WorldGeneration::kNoStratum) { return "None"; }
    return z <= Count() ? mSets[mDepthIndex].strata[z - 1].name : nullptr;
}

int StratumTable::IndexOf(const char* name) const
{
    if (name == nullptr) { return -1; }
    if (std::strcmp(name, "None") == 0) { return 0; }
    for (uint32_t i = 0; i < Count(); ++i)
    {
        if (std::strcmp(mSets[mDepthIndex].strata[i].name, name) == 0) { return static_cast<int>(i) + 1; }
    }
    return -1;
}

} // namespace World
} // namespace CoW
