#include <Shared/World/ZoneTable.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstring>
#include <memory>
#include <string>

namespace CoW {
namespace World {

namespace
{
    // Plain struct bound to the zone sidecar (ParamBinding requirement: standard layout).
    struct ZoneShares
    {
        float kFirstZoneShare = 0.0f;
        float kLastZoneShare = 0.0f;
    };

    bool Fail(ZoneError& err, const char* field, const char* reason)
    {
        err.field = field;
        err.reason = reason;
        DIA_LOG_ERROR("CoW", "ZoneTable: load failed at '%s': %s", field, reason);
        return false;
    }
}

bool ZoneTable::LoadFromText(const char* zonesText, const char* sidecarText, ZoneError& err)
{
    err = ZoneError();
    if (zonesText == nullptr) { return Fail(err, "document", "no zones text"); }
    if (sidecarText == nullptr) { return Fail(err, "sidecar", "no zone sidecar text"); }

    Json::Value root;
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        builder["rejectDupKeys"] = true;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        bool ok = false;
        try { ok = reader->parse(zonesText, zonesText + std::strlen(zonesText), &root, &errors); }
        catch (const std::exception&) { ok = false; }
        if (!ok) { return Fail(err, "document", "not valid JSON"); }
    }
    if (!root.isObject() || !root["format"].isString() || root["format"].asString() != "cow.zones")
    {
        return Fail(err, "format", "format must be \"cow.zones\"");
    }
    if (!root["version"].isUInt() || root["version"].asUInt() != 1) { return Fail(err, "version", "unsupported version"); }
    const Json::Value& zones = root["zones"];
    if (!zones.isArray()) { return Fail(err, "zones", "zones must be an array"); }
    if (zones.empty()) { return Fail(err, "zones", "at least one zone is required"); }
    if (zones.size() > kMaxZones) { return Fail(err, "zones", "more zones than the generator's three slots"); }

    ZoneTable cand;
    for (Json::ArrayIndex i = 0; i < zones.size(); ++i)
    {
        const Json::Value& z = zones[i];
        if (!z.isObject() || !z["name"].isString()) { return Fail(err, "zones", "each zone needs a string name"); }
        const std::string name = z["name"].asString();
        if (name.empty()) { return Fail(err, "zones", "zone name is empty"); }
        if (name.size() > kMaxNameLength) { return Fail(err, "zones", "zone name is too long"); }
        if (name == "None") { return Fail(err, "zones", "None is reserved for ocean"); }
        if (cand.IndexOf(name.c_str()) >= 0) { return Fail(err, "zones", "duplicate zone name"); }
        std::memcpy(cand.mNames[i], name.c_str(), name.size() + 1);
        ++cand.mCount;
    }

    Dia::Attribute::ParameterFile file;
    Dia::Attribute::ParamError perr;
    if (!file.Load(sidecarText, "zones.schema.json", nullptr, perr))
    {
        return Fail(err, "sidecar", perr.reason.AsCStr());
    }
    Dia::Attribute::ParamBinding<ZoneShares> binding;
    binding.Bind(Dia::Core::StringCRC("kFirstZoneShare"), &ZoneShares::kFirstZoneShare);
    binding.Bind(Dia::Core::StringCRC("kLastZoneShare"), &ZoneShares::kLastZoneShare);
    auto report = std::make_unique<Dia::Attribute::BindingReport>();
    if (!binding.Resolve(file.Schema(), *report)) { return Fail(err, "sidecar", "sidecar does not match the zone parameters"); }
    ZoneShares shares;
    binding.Snapshot(file.Values(), shares);
    cand.mFirstShare = shares.kFirstZoneShare;
    cand.mLastShare = shares.kLastZoneShare;

    *this = cand;
    DIA_LOG_INFO("CoW", "ZoneTable: loaded %u zones", cand.mCount);
    return true;
}

const char* ZoneTable::Name(WorldGeneration::DepthZone z) const
{
    if (z == WorldGeneration::kNoZone) { return "None"; }
    return z <= mCount ? mNames[z - 1] : nullptr;
}

int ZoneTable::IndexOf(const char* name) const
{
    if (name == nullptr) { return -1; }
    if (std::strcmp(name, "None") == 0) { return 0; }
    for (uint32_t i = 0; i < mCount; ++i)
    {
        if (std::strcmp(mNames[i], name) == 0) { return static_cast<int>(i) + 1; }
    }
    return -1;
}

} // namespace World
} // namespace CoW
