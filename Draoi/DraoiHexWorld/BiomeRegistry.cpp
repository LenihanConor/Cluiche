#include <DraoiHexWorld/BiomeRegistry.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace CoW {
namespace World {

namespace
{
    using Dia::Core::Containers::DynamicArrayC;
    using Dia::Core::Containers::String64;
    using Dia::Core::Containers::String256;
    using WorldGeneration::StratumIndex;

    // Tag vocabulary: TagId is the index in this list (at most 64).
    const char* const kTagNames[] = { "land", "water", "aquatic", "wet", "cold", "hot",
                                      "forest", "open", "fuel", "mountain", "volcanic", "succession" };
    constexpr uint32_t kTagCount = sizeof(kTagNames) / sizeof(kTagNames[0]);
    static_assert(kTagCount <= 64, "TagMask is 64 bits");

    const char* const kRoleNames[] = { "ocean", "lake", "fallback" };
    constexpr uint32_t kRoleCount = sizeof(kRoleNames) / sizeof(kRoleNames[0]);

    const char* const kPinNames[] = { "water_ocean", "water_lake", "vent", "alpine", "wetland" };

    constexpr uint32_t kMatrixBytes = (BiomeRegistry::kMaxBiomes + 7) / 8;   // 32

    template<size_t N>
    int IndexOf(const char* const (&names)[N], const char* text)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (std::strcmp(names[i], text) == 0) { return static_cast<int>(i); }
        }
        return -1;
    }

    struct Fnv
    {
        uint64_t h = 14695981039346656037ull;
        void Bytes(const void* p, size_t n)
        {
            const uint8_t* b = static_cast<const uint8_t*>(p);
            for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
        }
        template<class T> void Value(const T& v) { Bytes(&v, sizeof(T)); }
        void Float(float f) { if (f == 0.0f) { f = 0.0f; } Value(f); }   // -0.0 == 0.0, so it is rewritten as +0.0
    };
}

struct BiomeRegistry::Data
{
    struct Row
    {
        Dia::Core::StringCRC id;
        String64  idText;
        String64  name;
        String256 description;
        uint32_t  colour = 0;
        uint64_t  tagMask = 0;
        uint16_t  stratumMask = 0;
        bool      generated = false;
        bool      hasClimate = false;
        ClimateEnvelope climate = { 0.0f, 0.0f, 0.0f, 0.0f };
        Dia::Core::StringCRC pin;
        String64  pinText;
        int       pinPriority = -1;
        int8_t    plateau = -1;
    };

    DynamicArrayC<Row, kMaxBiomes> rows;
    StratumTable  zones;
    uint8_t    forbidden[kMaxBiomes][kMatrixBytes] = {};
    BiomeIndex climateList[kMaxBiomes] = {};
    uint32_t   climateCount = 0;
    BiomeIndex pinList[kMaxBiomes] = {};
    uint32_t   pinCount = 0;
    BiomeIndex roles[kRoleCount] = { kNoBiome, kNoBiome, kNoBiome };
    uint64_t   hash = 0;

    // Unknown ids already warned about by ResolveOrFallback (mutable: that method is const).
    static constexpr uint32_t kMaxWarnedIds = 64;
    mutable DynamicArrayC<Dia::Core::StringCRC, kMaxWarnedIds> warnedIds;

    const Row* At(BiomeIndex b) const { return b < rows.Size() ? &rows.At(b) : nullptr; }
    void ComputeHash();
};

void BiomeRegistry::Data::ComputeHash()
{
    Fnv f;
    const uint32_t count = rows.Size();
    f.Value(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Row& r = rows.At(i);
        f.Value(r.id.Value());
        f.Value(r.tagMask);
        // One byte as before (hash-stable for up to seven strata); the eighth stratum's bit adds a second byte.
        f.Value(static_cast<uint8_t>(r.stratumMask & 0xFFu));
        if ((r.stratumMask >> 8) != 0) { f.Value(static_cast<uint8_t>(r.stratumMask >> 8)); }
        f.Value(r.generated);
        f.Value(r.hasClimate);
        if (r.hasClimate)
        {
            f.Float(r.climate.t0); f.Float(r.climate.m0); f.Float(r.climate.wt); f.Float(r.climate.wm);
        }
        // The five original rules hash as their historic enum index so existing registry hashes (world JSON) stay stable.
        const int legacyRule = IndexOf(kPinNames, r.pinText.AsCStr());
        f.Value(static_cast<uint8_t>(legacyRule >= 0 ? legacyRule : (r.pinPriority < 0 ? 0 : 0xFF))); // unpinned rows hashed as the old default (WaterOcean = 0)
        if (legacyRule < 0 && r.pinPriority >= 0) { f.Value(r.pin.Value()); }
        f.Value(r.pinPriority);
        f.Value(r.plateau);
    }
    f.Bytes(roles, sizeof(roles));
    for (uint32_t i = 0; i < count; ++i) { f.Bytes(forbidden[i], kMatrixBytes); }
    hash = f.h;
}

BiomeRegistry::BiomeRegistry() = default;
BiomeRegistry::~BiomeRegistry() = default;
BiomeRegistry::BiomeRegistry(BiomeRegistry&&) noexcept = default;
BiomeRegistry& BiomeRegistry::operator=(BiomeRegistry&&) noexcept = default;
void BiomeRegistry::Swap(BiomeRegistry& other) noexcept { mData.swap(other.mData); }

namespace
{
    bool Fail(RegistryError& err, const char* field, const char* id, uint32_t row, const char* reason)
    {
        err.field.SetText(field);
        err.id.SetText(id);
        err.row = row;
        err.reason.SetText(reason);
        DIA_LOG_ERROR("CoW", "BiomeRegistry: load failed at row %u (%s, id '%s'): %s", row, field, id, reason);
        return false;
    }

    bool ParseColour(const std::string& text, uint32_t& out)
    {
        if (text.size() != 7 || text[0] != '#') { return false; }
        uint32_t v = 0;
        for (size_t i = 1; i < 7; ++i)
        {
            const char c = text[i];
            uint32_t d;
            if (c >= '0' && c <= '9') { d = c - '0'; }
            else if (c >= 'a' && c <= 'f') { d = 10 + c - 'a'; }
            else if (c >= 'A' && c <= 'F') { d = 10 + c - 'A'; }
            else { return false; }
            v = (v << 4) | d;
        }
        out = v;
        return true;
    }

    bool ReadFloat(const Json::Value& obj, const char* key, float& out)
    {
        const Json::Value& v = obj[key];
        if (!v.isNumeric()) { return false; }
        out = static_cast<float>(v.asDouble());
        return std::isfinite(out);
    }

    // Reads an array of strings into `out` (cleared first). Returns false if not an array of strings.
    bool ReadStrings(const Json::Value& v, std::vector<std::string>& out)
    {
        out.clear();
        if (!v.isArray()) { return false; }
        for (const Json::Value& e : v)
        {
            if (!e.isString()) { return false; }
            out.push_back(e.asString());
        }
        return true;
    }

    bool ParseRow(const Json::Value& jr, uint32_t rowIndex, BiomeRegistry::Data& d, RegistryError& err, PinRuleValidator knownPinRule)
    {
        using Row = BiomeRegistry::Data::Row;
        if (!jr.isObject()) { return Fail(err, "biomes", "", rowIndex, "row is not an object"); }
        if (!jr["id"].isString() || jr["id"].asString().empty() || jr["id"].asString().size() >= String64::Capacity())
        {
            return Fail(err, "id", "", rowIndex, "id missing, empty or too long");
        }
        const std::string id = jr["id"].asString();
        const char* idc = id.c_str();

        for (uint32_t i = 0; i < d.rows.Size(); ++i)
        {
            if (std::strcmp(d.rows.At(i).idText.AsCStr(), idc) == 0) { return Fail(err, "id", idc, rowIndex, "duplicate id"); }
        }

        d.rows.AddDefault();
        Row& r = d.rows.Back();
        r.idText.SetText(idc);
        r.id = Dia::Core::StringCRC(idc);

        if (!jr["name"].isString() || jr["name"].asString().size() >= String64::Capacity())
        {
            return Fail(err, "name", idc, rowIndex, "name missing or too long");
        }
        r.name.SetText(jr["name"].asString().c_str());

        if (!jr["description"].isString() || jr["description"].asString().size() >= String256::Capacity())
        {
            return Fail(err, "description", idc, rowIndex, "description missing or too long");
        }
        r.description.SetText(jr["description"].asString().c_str());

        if (!jr["colour"].isString() || !ParseColour(jr["colour"].asString(), r.colour))
        {
            return Fail(err, "colour", idc, rowIndex, "colour must be '#rrggbb'");
        }
        if (!jr["generated"].isBool()) { return Fail(err, "generated", idc, rowIndex, "generated must be a bool"); }
        r.generated = jr["generated"].asBool();

        std::vector<std::string> names;
        if (!ReadStrings(jr["tags"], names)) { return Fail(err, "tags", idc, rowIndex, "tags must be an array of strings"); }
        for (const std::string& t : names)
        {
            const int tag = IndexOf(kTagNames, t.c_str());
            if (tag < 0) { return Fail(err, "tags", idc, rowIndex, "unknown tag"); }
            r.tagMask |= 1ull << tag;
        }

        if (!ReadStrings(jr["zones"], names)) { return Fail(err, "zones", idc, rowIndex, "zones must be an array of strings"); }
        for (const std::string& z : names)
        {
            const int zone = d.zones.IndexOf(z.c_str());
            if (zone < 0) { return Fail(err, "zones", idc, rowIndex, "unknown zone"); }
            r.stratumMask |= static_cast<uint16_t>(1u << zone);
        }

        if (jr.isMember("roles"))
        {
            if (!ReadStrings(jr["roles"], names)) { return Fail(err, "roles", idc, rowIndex, "roles must be an array of strings"); }
            for (const std::string& n : names)
            {
                const int role = IndexOf(kRoleNames, n.c_str());
                if (role < 0) { return Fail(err, "roles", idc, rowIndex, "unknown role"); }
                if (d.roles[role] != kNoBiome) { return Fail(err, "roles", idc, rowIndex, "role is held by more than one row"); }
                d.roles[role] = static_cast<BiomeIndex>(rowIndex);
            }
        }

        if (jr.isMember("climate"))
        {
            const Json::Value& c = jr["climate"];
            if (!c.isObject() || !ReadFloat(c, "t0", r.climate.t0) || !ReadFloat(c, "m0", r.climate.m0) ||
                !ReadFloat(c, "wt", r.climate.wt) || !ReadFloat(c, "wm", r.climate.wm))
            {
                return Fail(err, "climate", idc, rowIndex, "climate needs numeric t0, m0, wt, wm");
            }
            if (!(r.climate.wt > 0.0f)) { return Fail(err, "climate.wt", idc, rowIndex, "climate wt must be > 0"); }
            if (!(r.climate.wm > 0.0f)) { return Fail(err, "climate.wm", idc, rowIndex, "climate wm must be > 0"); }
            r.hasClimate = true;
        }

        if (jr.isMember("pin"))
        {
            const Json::Value& p = jr["pin"];
            if (!p.isObject() || !p["rule"].isString() || p["rule"].asString().empty() ||
                p["rule"].asString().size() >= String64::Capacity())
            {
                return Fail(err, "pin", idc, rowIndex, "missing pin rule");
            }
            const std::string rule = p["rule"].asString();
            const Dia::Core::StringCRC ruleId(rule.c_str());
            if (knownPinRule != nullptr && !knownPinRule(ruleId)) { return Fail(err, "pin", idc, rowIndex, "unknown pin rule (no predicate registered for it)"); }
            if (!p["priority"].isInt() || p["priority"].asInt() < 0) { return Fail(err, "pin", idc, rowIndex, "pin priority must be an integer >= 0"); }
            for (uint32_t i = 0; i < rowIndex; ++i)
            {
                if (d.rows.At(i).pinPriority == p["priority"].asInt()) { return Fail(err, "pin.priority", idc, rowIndex, "pin priority already used by another row"); }
            }
            r.pin = ruleId;
            r.pinText.SetText(rule.c_str());
            r.pinPriority = p["priority"].asInt();
        }

        if (jr.isMember("plateau"))
        {
            const std::string p = jr["plateau"].isString() ? jr["plateau"].asString() : std::string();
            if (p == "dry") { r.plateau = 0; }
            else if (p == "wet") { r.plateau = 1; }
            else { return Fail(err, "plateau", idc, rowIndex, "plateau must be 'wet' or 'dry'"); }
            for (uint32_t i = 0; i < rowIndex; ++i)
            {
                if (d.rows.At(i).plateau == r.plateau) { return Fail(err, "plateau", idc, rowIndex, "plateau already held by another row"); }
            }
        }
        return true;
    }
}

bool BiomeRegistry::LoadFromText(const char* json, const StratumTable& zones, RegistryError& err, PinRuleValidator knownPinRule)
{
    if (json == nullptr) { return Fail(err, "document", "", 0, "no text"); }

    Json::Value root;
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        builder["rejectDupKeys"] = true;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        try
        {
            if (!reader->parse(json, json + std::strlen(json), &root, &errors))
            {
                return Fail(err, "document", "", 0, "not valid JSON");
            }
        }
        catch (const std::exception&)
        {
            return Fail(err, "document", "", 0, "not valid JSON");
        }
    }

    if (!root.isObject() || !root["format"].isString() || root["format"].asString() != "cow.biomes")
    {
        return Fail(err, "format", "", 0, "format must be 'cow.biomes'");
    }
    if (!root["version"].isInt() || root["version"].asInt() != 1)
    {
        return Fail(err, "version", "", 0, "version must be 1");
    }
    const Json::Value& biomes = root["biomes"];
    if (!biomes.isArray()) { return Fail(err, "biomes", "", 0, "biomes must be an array"); }
    if (biomes.size() > kMaxBiomes) { return Fail(err, "biomes", "", 0, "more than 254 biomes"); }

    std::unique_ptr<Data> cand(new Data());   // heap: about 120 KB
    cand->zones = zones;
    uint32_t row = 0;
    for (const Json::Value& jr : biomes)
    {
        if (!ParseRow(jr, row, *cand, err, knownPinRule)) { return false; }
        ++row;
    }

    for (uint32_t role = 0; role < kRoleCount; ++role)
    {
        if (cand->roles[role] == kNoBiome) { return Fail(err, "roles", "", 0, "a role is held by no row (ocean, lake, fallback each need exactly one)"); }
    }

    // Forbidden pairs, stored symmetric.
    const Json::Value& forbidden = root["forbidden"];
    if (!forbidden.isNull())
    {
        if (!forbidden.isArray()) { return Fail(err, "forbidden", "", 0, "forbidden must be an array of pairs"); }
        uint32_t pairIndex = 0;
        for (const Json::Value& pair : forbidden)
        {
            if (!pair.isArray() || pair.size() != 2 || !pair[0].isString() || !pair[1].isString())
            {
                return Fail(err, "forbidden", "", pairIndex, "each forbidden entry must be a pair of ids");
            }
            const std::string a = pair[0].asString();
            const std::string b = pair[1].asString();
            uint32_t ia = kNoBiome, ib = kNoBiome;
            for (uint32_t i = 0; i < cand->rows.Size(); ++i)
            {
                if (std::strcmp(cand->rows.At(i).idText.AsCStr(), a.c_str()) == 0) { ia = i; }
                if (std::strcmp(cand->rows.At(i).idText.AsCStr(), b.c_str()) == 0) { ib = i; }
            }
            if (ia == kNoBiome || ib == kNoBiome)
            {
                return Fail(err, "forbidden", ia == kNoBiome ? a.c_str() : b.c_str(), pairIndex, "forbidden pair names an unknown id");
            }
            if (ia == ib) { return Fail(err, "forbidden", a.c_str(), pairIndex, "biome is forbidden with itself"); }
            if (((cand->forbidden[ia][ib >> 3] >> (ib & 7)) & 1u) != 0) { return Fail(err, "forbidden", a.c_str(), pairIndex, "forbidden pair listed twice"); }
            cand->forbidden[ia][ib >> 3] |= static_cast<uint8_t>(1u << (ib & 7));
            cand->forbidden[ib][ia >> 3] |= static_cast<uint8_t>(1u << (ia & 7));
            ++pairIndex;
        }
    }

    // Derived tables.
    const uint32_t count = cand->rows.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        if (cand->rows.At(i).hasClimate) { cand->climateList[cand->climateCount++] = static_cast<BiomeIndex>(i); }
        if (cand->rows.At(i).pinPriority >= 0) { cand->pinList[cand->pinCount++] = static_cast<BiomeIndex>(i); }
    }
    for (uint32_t i = 1; i < cand->pinCount; ++i)   // insertion sort, stable, priority descending
    {
        const BiomeIndex key = cand->pinList[i];
        uint32_t j = i;
        while (j > 0 && cand->rows.At(cand->pinList[j - 1]).pinPriority < cand->rows.At(key).pinPriority)
        {
            cand->pinList[j] = cand->pinList[j - 1];
            --j;
        }
        cand->pinList[j] = key;
    }
    cand->ComputeHash();

    DIA_LOG_INFO("CoW", "BiomeRegistry: loaded %u biomes, hash %016llx", count, static_cast<unsigned long long>(cand->hash));
    mData = std::move(cand);
    return true;
}

bool BiomeRegistry::Load(const Dia::Core::FilePath& path, const StratumTable& zones, RegistryError& err, PinRuleValidator knownPinRule)
{
    Dia::Core::FilePath::ResoledFilePath resolved;
    path.Resolve(resolved);
    std::ifstream in(resolved.AsCStr(), std::ios::binary);
    if (!in.good()) { return Fail(err, "file", resolved.AsCStr(), 0, "cannot read file"); }
    std::stringstream ss;
    ss << in.rdbuf();
    return LoadFromText(ss.str().c_str(), zones, err, knownPinRule);
}

const StratumTable& BiomeRegistry::Strata() const
{
    static const StratumTable kEmpty;
    return mData ? mData->zones : kEmpty;
}

bool BiomeRegistry::SetStrataParams(const std::vector<std::pair<std::string, double>>& changes, StratumError& err, bool* changed)
{
    if (!mData)
    {
        err = StratumError{ "strata", "no biome registry is loaded" };
        if (changed != nullptr) { *changed = false; }
        return false;
    }
    return mData->zones.SetParams(changes, err, changed);
}

uint32_t BiomeRegistry::Count() const { return mData ? mData->rows.Size() : 0; }

BiomeIndex BiomeRegistry::Find(Dia::Core::StringCRC id) const
{
    if (!mData) { return kNoBiome; }
    for (uint32_t i = 0; i < mData->rows.Size(); ++i)
    {
        if (mData->rows.At(i).id == id) { return static_cast<BiomeIndex>(i); }
    }
    return kNoBiome;
}

Dia::Core::StringCRC BiomeRegistry::Id(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->id : Dia::Core::StringCRC();
}

const char* BiomeRegistry::IdText(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->idText.AsCStr() : "";
}

const char* BiomeRegistry::Name(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->name.AsCStr() : "";
}

uint32_t BiomeRegistry::Colour(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->colour : 0u;
}

bool BiomeRegistry::HasTag(BiomeIndex b, TagId tag) const
{
    return tag < 64 && ((TagMask(b) >> tag) & 1ull) != 0;
}

uint64_t BiomeRegistry::TagMask(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->tagMask : 0ull;
}

TagId BiomeRegistry::FindTag(Dia::Core::StringCRC tag) const
{
    for (uint32_t i = 0; i < kTagCount; ++i)
    {
        if (Dia::Core::StringCRC(kTagNames[i]) == tag) { return static_cast<TagId>(i); }
    }
    return kNoTag;
}

BiomeIndex BiomeRegistry::ResolveOrFallback(Dia::Core::StringCRC id) const
{
    if (!mData) { return kNoBiome; }

    const BiomeIndex found = Find(id);
    if (found != kNoBiome) { return found; }

    // Warn once per distinct unknown id. The list is capped (kMaxWarnedIds); past the cap new
    // ids fall back silently rather than warning on every call.
    bool warned = false;
    for (uint32_t i = 0; i < mData->warnedIds.Size(); ++i)
    {
        if (mData->warnedIds.At(i) == id) { warned = true; break; }
    }
    if (!warned && !mData->warnedIds.IsFull())
    {
        mData->warnedIds.Add(id);
        DIA_LOG_WARNING("CoW", "BiomeRegistry unknown id '%s' (crc 0x%08X), using fallback biome",
                        id.AsChar(), static_cast<unsigned int>(id.Value()));
    }
    return mData->roles[static_cast<uint32_t>(BiomeRole::Fallback)];
}

BiomeIndex BiomeRegistry::Role(BiomeRole role) const
{
    const uint32_t i = static_cast<uint32_t>(role);
    return (mData && i < kRoleCount) ? mData->roles[i] : kNoBiome;
}

bool BiomeRegistry::IsGenerated(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r && r->generated;
}

bool BiomeRegistry::IsValidInStratum(BiomeIndex b, StratumIndex zone) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    const uint32_t z = static_cast<uint32_t>(zone);
    return r && z <= mData->zones.Count() && ((r->stratumMask >> z) & 1u) != 0;
}

bool BiomeRegistry::AreForbidden(BiomeIndex a, BiomeIndex b) const
{
    if (!mData || !mData->At(a) || !mData->At(b)) { return false; }
    return ((mData->forbidden[a][b >> 3] >> (b & 7)) & 1u) != 0;
}

const ClimateEnvelope* BiomeRegistry::Climate(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return (r && r->hasClimate) ? &r->climate : nullptr;
}

const BiomeIndex* BiomeRegistry::ClimateBiomes(uint32_t& count) const
{
    count = mData ? mData->climateCount : 0;
    return (mData && count > 0) ? mData->climateList : nullptr;
}

const BiomeIndex* BiomeRegistry::PinsByPriority(uint32_t& count) const
{
    count = mData ? mData->pinCount : 0;
    return (mData && count > 0) ? mData->pinList : nullptr;
}

Dia::Core::StringCRC BiomeRegistry::Pin(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->pin : Dia::Core::StringCRC();
}

const char* BiomeRegistry::PinRuleText(BiomeIndex b) const
{
    const Data::Row* r = mData ? mData->At(b) : nullptr;
    return r ? r->pinText.AsCStr() : "";
}

BiomeIndex BiomeRegistry::Plateau(bool wet) const
{
    if (!mData) { return kNoBiome; }
    const int8_t want = wet ? 1 : 0;
    for (uint32_t i = 0; i < mData->rows.Size(); ++i)
    {
        if (mData->rows.At(i).plateau == want) { return static_cast<BiomeIndex>(i); }
    }
    return kNoBiome;
}

uint64_t BiomeRegistry::Hash() const { return mData ? mData->hash : 0ull; }

} // namespace World
} // namespace CoW
