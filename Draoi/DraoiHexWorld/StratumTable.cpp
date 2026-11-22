#include <DraoiHexWorld/StratumTable.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

namespace Draoi {
namespace HexWorld {

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
        DIA_LOG_ERROR("HexWorld", "StratumTable: load failed at '%s': %s", field.c_str(), reason.c_str());
        return false;
    }

    bool CopyName(char (&dst)[32], const std::string& name)
    {
        if (name.empty() || name.size() > StratumTable::kMaxNameLength) { return false; }
        std::memcpy(dst, name.c_str(), name.size() + 1);
        return true;
    }

    const char* AttributeTypeName(StratumAttributeType t)
    {
        return t == StratumAttributeType::Float ? "float" : t == StratumAttributeType::Int ? "int" : "bool";
    }

    // Reads a JSON value as the attribute's type within its range. A bool attribute takes only true/false, a number never a bool.
    bool ReadAttributeValue(const StratumAttributeDecl& a, const Json::Value& v, float& out)
    {
        double d = 0.0;
        switch (a.type)
        {
        case StratumAttributeType::Bool:
            if (!v.isBool()) { return false; }
            out = v.asBool() ? 1.0f : 0.0f;
            return true;
        case StratumAttributeType::Int:
            if (v.isBool() || !v.isIntegral()) { return false; }
            d = static_cast<double>(v.asInt64());
            break;
        case StratumAttributeType::Float:
            if (v.isBool() || !v.isNumeric()) { return false; }
            d = v.asDouble();
            break;
        }
        if (!(d >= static_cast<double>(a.min) && d <= static_cast<double>(a.max))) { return false; }
        out = static_cast<float>(d);
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

bool StratumTable::LoadFromText(const char* strataText, const char* sidecarText, StratumError& err, const AssignerTypeCheck& knownAssignerType,
                                const MetricCheck& knownMetric)
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
            if (!assigner["metric"].isString() || assigner["metric"].asString().empty() || assigner["metric"].asString().size() > kMaxNameLength)
            {
                return Fail(err, where + ".assigner", "set '" + id + "': metric must be a non-empty string");
            }
            const std::string metric = assigner["metric"].asString();
            const bool builtin = metric == kMetricCoastCost || metric == kMetricElevation || metric == kMetricLatitude;
            if (!(knownMetric ? knownMetric(metric.c_str()) : builtin))
            {
                return Fail(err, where + ".assigner.metric", "set '" + id + "': unknown metric '" + metric + "' (not " + kMetricCoastCost +
                                                              ", " + kMetricElevation + ", " + kMetricLatitude + " or a registered cell field)");
            }
            std::memcpy(set.metric, metric.c_str(), metric.size() + 1);
        }
        const bool coastMetric = std::strcmp(set.metric, kMetricCoastCost) == 0;

        if (js.isMember("attribute_schema"))
        {
            const Json::Value& schema = js["attribute_schema"];
            if (!schema.isObject()) { return Fail(err, where + ".attribute_schema", "set '" + id + "': attribute_schema must be an object"); }
            if (schema.size() > StratumSpec::kMaxAttributes)
            {
                return Fail(err, where + ".attribute_schema", "set '" + id + "': more than " + std::to_string(StratumSpec::kMaxAttributes) + " attributes");
            }
            for (const std::string& an : schema.getMemberNames())
            {
                const std::string awhere = where + ".attribute_schema." + an;
                const Json::Value& ja = schema[an];
                StratumAttributeDecl& a = set.attributes[set.attributeCount];
                if (an.empty() || an.size() > kMaxNameLength || !ja.isObject() || !ja["type"].isString())
                {
                    return Fail(err, awhere, "set '" + id + "': attribute '" + an + "' needs a name up to 31 characters and a string type");
                }
                std::memcpy(a.name, an.c_str(), an.size() + 1);
                const std::string type = ja["type"].asString();
                if (type == "float") { a.type = StratumAttributeType::Float; }
                else if (type == "int") { a.type = StratumAttributeType::Int; }
                else if (type == "bool") { a.type = StratumAttributeType::Bool; }
                else
                {
                    return Fail(err, awhere, "set '" + id + "': attribute '" + an + "' has unsupported type '" + type + "' (float, int or bool; DiaAttribute has no strings)");
                }
                if (a.type == StratumAttributeType::Bool)
                {
                    a.min = 0.0f;
                    a.max = 1.0f;
                }
                else
                {
                    if (!ja["min"].isNumeric() || !ja["max"].isNumeric() || ja["min"].isBool() || ja["max"].isBool() ||
                        ja["min"].asDouble() > ja["max"].asDouble())
                    {
                        return Fail(err, awhere, "set '" + id + "': attribute '" + an + "' needs numeric min <= max");
                    }
                    a.min = static_cast<float>(ja["min"].asDouble());
                    a.max = static_cast<float>(ja["max"].asDouble());
                }
                if (!ja.isMember("default")) { a.def = std::max(a.min, std::min(0.0f, a.max)); }
                else if (!ReadAttributeValue(a, ja["default"], a.def))
                {
                    return Fail(err, awhere, "set '" + id + "': attribute '" + an + "' default is not a " + type + " within " +
                                             std::to_string(a.min) + ".." + std::to_string(a.max));
                }
                ++set.attributeCount;
            }
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
                if (z.mainLandmassOnly && !coastMetric)
                {
                    return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': main_landmass_only is a coast_cost rule but the set's metric is '" + set.metric + "'");
                }
            }
            for (uint32_t a = 0; a < set.attributeCount; ++a) { z.attributeValue[a] = set.attributes[a].def; }
            if (jz.isMember("attributes"))
            {
                if (!jz["attributes"].isObject()) { return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': attributes must be an object"); }
                for (const std::string& an : jz["attributes"].getMemberNames())
                {
                    uint32_t a = 0;
                    while (a < set.attributeCount && an != set.attributes[a].name) { ++a; }
                    if (a == set.attributeCount)
                    {
                        return Fail(err, zwhere + ".attributes." + an, "stratum '" + name + "' of set '" + id + "': unknown attribute '" + an + "' (the set's attribute_schema does not declare it)");
                    }
                    if (!ReadAttributeValue(set.attributes[a], jz["attributes"][an], z.attributeValue[a]))
                    {
                        return Fail(err, zwhere + ".attributes." + an, "stratum '" + name + "' of set '" + id + "': attribute '" + an + "' is not a valid " +
                                                                       AttributeTypeName(set.attributes[a].type) + " within " +
                                                                       std::to_string(set.attributes[a].min) + ".." + std::to_string(set.attributes[a].max));
                    }
                }
            }
            if (jz.isMember("min_coast_depth"))
            {
                if (!coastMetric)
                {
                    return Fail(err, zwhere, "stratum '" + name + "' of set '" + id + "': min_coast_depth is a coast_cost rule but the set's metric is '" + set.metric + "'");
                }
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

    // Sidecar: the parameter file owning every tunable number (shares, minimum depths, attribute values).
    auto file = std::make_shared<Dia::Attribute::ParameterFile>();
    Dia::Attribute::ParamError perr;
    if (!file->Load(sidecarText, "strata.schema.json", nullptr, perr))
    {
        return Fail(err, "sidecar", perr.reason.AsCStr());
    }
    if (!ApplySidecar(cand, *file, err)) { return false; }
    cand.mSidecarText = sidecarText;
    cand.mSidecar = file;

    *this = cand;
    DIA_LOG_INFO("HexWorld", "StratumTable: loaded %u sets, depth set has %u strata", cand.mSetCount, cand.Count());
    return true;
}

bool StratumTable::ApplySidecar(StratumTable& cand, const Dia::Attribute::ParameterFile& file, StratumError& err)
{
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
            for (uint32_t a = 0; a < set.attributeCount; ++a)
            {
                const StratumAttributeDecl& decl = set.attributes[a];
                const std::string attrKey = std::string(set.id) + "." + set.strata[i].name + ".attr." + decl.name;
                const Dia::Core::StringCRC crc(attrKey.c_str());
                const Dia::Attribute::AttributeDefinition* def = file.Schema().FindAttribute(crc);
                const Dia::Attribute::AttributeKind want = decl.type == StratumAttributeType::Float ? Dia::Attribute::AttributeKind::Float
                                                         : decl.type == StratumAttributeType::Int   ? Dia::Attribute::AttributeKind::Int
                                                                                                    : Dia::Attribute::AttributeKind::Bool;
                if (def == nullptr || def->kind != want)
                {
                    return Fail(err, "sidecar", "sidecar has no usable " + std::string(AttributeTypeName(decl.type)) + " parameter '" + attrKey + "'");
                }
                const float value = file.Values().GetValue(crc);
                if (!(value >= decl.min && value <= decl.max))
                {
                    return Fail(err, "sidecar", "parameter '" + attrKey + "' is " + std::to_string(value) + ", outside the attribute's range " +
                                                std::to_string(decl.min) + ".." + std::to_string(decl.max));
                }
                set.strata[i].attributeValue[a] = value;
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
    return true;
}

bool StratumTable::SetParams(const std::vector<std::pair<std::string, double>>& changes, StratumError& err, bool* changed)
{
    err = StratumError();
    if (changed != nullptr) { *changed = false; }
    if (!mSidecar) { return Fail(err, "sidecar", "no strata table is loaded"); }

    std::vector<std::pair<std::string, double>> overrides = mOverrides;
    for (const auto& c : changes)
    {
        auto it = std::find_if(overrides.begin(), overrides.end(), [&](const auto& o) { return o.first == c.first; });
        if (it != overrides.end()) { it->second = c.second; }
        else { overrides.push_back(c); }
    }
    auto file = std::make_shared<Dia::Attribute::ParameterFile>();
    Dia::Attribute::ParamError perr;
    if (!file->Load(mSidecarText.c_str(), "strata.schema.json", nullptr, perr)) { return Fail(err, "sidecar", perr.reason.AsCStr()); }
    for (const auto& o : overrides)
    {
        if (!std::isfinite(o.second)) { return Fail(err, o.first, "parameter '" + o.first + "': value is not a finite number"); }
        if (!file->Set(Dia::Core::StringCRC(o.first.c_str()), Json::Value(o.second), perr))
        {
            return Fail(err, o.first, std::string("parameter '") + o.first + "': " + perr.reason.AsCStr());
        }
    }
    if (file->Hash() == mSidecar->Hash()) { return true; }   // every value equals the current one

    StratumTable cand = *this;
    if (!ApplySidecar(cand, *file, err)) { return false; }
    cand.mOverrides = overrides;
    cand.mSidecar = file;
    *this = cand;
    if (changed != nullptr) { *changed = true; }
    DIA_LOG_INFO("HexWorld", "StratumTable: %u sidecar parameter(s) changed live", static_cast<unsigned>(changes.size()));
    return true;
}

bool StratumTable::TryAttribute(uint32_t setIndex, StratumIndex z, const char* name, float& out) const
{
    if (name == nullptr || setIndex >= mSetCount || z == kNoStratum || z > mSets[setIndex].count) { return false; }
    const StratumSetSpec& set = mSets[setIndex];
    for (uint32_t a = 0; a < set.attributeCount; ++a)
    {
        if (std::strcmp(set.attributes[a].name, name) == 0)
        {
            out = set.strata[z - 1].attributeValue[a];
            return true;
        }
    }
    return false;
}

float StratumTable::Attribute(uint32_t setIndex, StratumIndex z, const char* name) const
{
    float v = 0.0f;
    return TryAttribute(setIndex, z, name, v) ? v : 0.0f;
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

int StratumTable::SetIndexOf(const char* id) const
{
    if (id == nullptr) { return -1; }
    for (uint32_t i = 0; i < mSetCount; ++i)
    {
        if (std::strcmp(mSets[i].id, id) == 0) { return static_cast<int>(i); }
    }
    return -1;
}

const char* StratumTable::NameIn(const StratumSetSpec& set, StratumIndex z)
{
    if (z == kNoStratum) { return "None"; }
    return z <= set.count ? set.strata[z - 1].name : nullptr;
}

int StratumTable::IndexIn(const StratumSetSpec& set, const char* name)
{
    if (name == nullptr) { return -1; }
    if (std::strcmp(name, "None") == 0) { return 0; }
    for (uint32_t i = 0; i < set.count; ++i)
    {
        if (std::strcmp(set.strata[i].name, name) == 0) { return static_cast<int>(i) + 1; }
    }
    return -1;
}

const char* StratumTable::Name(StratumIndex z) const
{
    if (z == kNoStratum) { return "None"; }
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

} // namespace HexWorld
} // namespace Draoi
