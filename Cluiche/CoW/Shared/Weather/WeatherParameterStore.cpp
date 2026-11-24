#include <Shared/Weather/WeatherParameterStore.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DraoiHexWorld/BiomeRegistry.h>

#include <cstring>
#include <fstream>
#include <sstream>

namespace CoW {
namespace Weather {

using Dia::Attribute::ParamWarnings;
using Draoi::HexWorld::Weather::WeatherParameters;

namespace {
constexpr const char* kBiomeKeyRef = "cow.biomes/biomes[].id";
constexpr const char* kSidecarName = "weather_parameters";
constexpr const char* kChannel = "WTH";

void CopyText(char* dst, size_t cap, const char* src)
{
    std::strncpy(dst, src != nullptr ? src : "", cap - 1);
    dst[cap - 1] = '\0';
}

void LogWarnings(const ParamWarnings& warn)
{
    for (uint32_t i = 0; i < warn.items.Size(); ++i)
    {
        DIA_LOG_WARNING(kChannel, "WTH: parameter pack warning at '%s': %s", warn.items[i].key.AsCStr(),
                        warn.items[i].reason.AsCStr());
    }
}
}

void ParamError::Set(const char* k, const char* r)
{
    CopyText(key, sizeof(key), k);
    CopyText(reason, sizeof(reason), r);
}

bool FileSidecarSource::Read(std::string& text, std::string& error)
{
    std::ifstream in(mPath, std::ios::binary);
    if (!in.good())
    {
        error = "cannot read " + mPath;
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
}

void WeatherParameterStore::RegistryKeys::Build(const Draoi::HexWorld::BiomeRegistry& registry)
{
    mIds.clear();
    mNames.clear();
    mNamePtrs.clear();
    for (uint32_t i = 0; i < registry.Count(); ++i)
    {
        mIds.push_back(registry.Id(static_cast<Draoi::HexWorld::BiomeIndex>(i)));
        mNames.push_back(registry.IdText(static_cast<Draoi::HexWorld::BiomeIndex>(i)));
    }
    for (const std::string& n : mNames) { mNamePtrs.push_back(n.c_str()); }
}

bool WeatherParameterStore::RegistryKeys::GetKeys(const char* keyRef, Dia::Attribute::MapKeys& out) const
{
    if (keyRef == nullptr || std::strcmp(keyRef, kBiomeKeyRef) != 0) { return false; }
    out.ids = mIds.data();
    out.names = mNamePtrs.data();
    out.count = static_cast<uint32_t>(mIds.size());
    return true;
}

WeatherParameterStore::WeatherParameterStore() {}
WeatherParameterStore::~WeatherParameterStore() {}

void WeatherParameterStore::Copy(const Dia::Attribute::ParamError& from, ParamError& to)
{
    CopyText(to.key, sizeof(to.key), from.key.AsCStr());
    CopyText(to.reason, sizeof(to.reason), from.reason.AsCStr());
    if (to.key[0] == '\0') { CopyText(to.key, sizeof(to.key), "sidecar"); }
}

bool WeatherParameterStore::Bind(Dia::Attribute::ParamBinding<WeatherParameters>& binding,
                                 const Dia::Attribute::ParameterFile& file, ParamError& err) const
{
    Draoi::HexWorld::Weather::BindWeatherParameters(binding, mRegistry->Count());
    Dia::Attribute::BindingReport report;
    if (binding.Resolve(file.Schema(), report)) { return true; }
    const char* key = "sidecar";
    const char* what = "binding failed";
    if (report.unboundParameters.Size() > 0) { key = report.unboundParameters[0].AsCStr(); what = "parameter has no binding"; }
    else if (report.bindingsWithoutEntry.Size() > 0) { key = report.bindingsWithoutEntry[0].AsCStr(); what = "binding has no sidecar entry"; }
    else if (report.countMismatches.Size() > 0) { key = report.countMismatches[0].AsCStr(); what = "map count differs from the biome registry"; }
    else if (report.kindMismatches.Size() > 0) { key = report.kindMismatches[0].AsCStr(); what = "binding kind differs from the sidecar kind"; }
    err.Set(key, what);
    return false;
}

bool WeatherParameterStore::LoadInto(Dia::Attribute::ParameterFile& file, const std::string& text, ParamError& err)
{
    Dia::Attribute::ParamError perr;
    if (!file.Load(text.c_str(), kSidecarName, &mKeys, perr))
    {
        Copy(perr, err);
        return false;
    }
    for (const Json::Value& pack : mPacks)
    {
        ParamWarnings warn;
        if (!file.ApplyPack(pack, warn, perr))
        {
            Copy(perr, err);
            return false;
        }
        LogWarnings(warn);
    }
    return true;
}

bool WeatherParameterStore::Load(IWeatherSidecarSource& source, const Draoi::HexWorld::BiomeRegistry& registry, ParamError& err)
{
    err.Clear();
    mLoaded = false;
    mFile.reset();
    mProbe.reset();
    mSource = &source;
    mRegistry = &registry;
    mKeys.Build(registry);

    std::string text, error;
    if (!source.Read(text, error))
    {
        err.Set("sidecar", error.c_str());
        return false;
    }
    std::unique_ptr<Dia::Attribute::ParameterFile> file(new Dia::Attribute::ParameterFile());
    if (!LoadInto(*file, text, err)) { return false; }
    std::unique_ptr<Dia::Attribute::ParamBinding<WeatherParameters>> binding(new Dia::Attribute::ParamBinding<WeatherParameters>());
    if (!Bind(*binding, *file, err)) { return false; }
    std::unique_ptr<Dia::Attribute::ParameterFile> probe(new Dia::Attribute::ParameterFile());
    if (!LoadInto(*probe, text, err)) { return false; }

    mFile = std::move(file);
    mProbe = std::move(probe);
    mBinding = std::move(binding);
    mText = text;
    mLoaded = true;
    return true;
}

void WeatherParameterStore::Snapshot(WeatherParameters& out) const
{
    if (mLoaded) { mBinding->Snapshot(mFile->Values(), out); }
}

uint64_t WeatherParameterStore::Hash() const
{
    return mLoaded ? mFile->Hash() : 0;
}

bool WeatherParameterStore::CheckSet(const char* dottedKey, const Json::Value& value, ParamError& err)
{
    err.Clear();
    if (!mLoaded)
    {
        err.Set(dottedKey, "parameters are not loaded");
        return false;
    }
    Dia::Attribute::ParamError perr;
    const Dia::Core::StringCRC key(dottedKey);
    const bool known = mProbe->Schema().FindAttribute(key) != nullptr;
    const float before = known ? mProbe->Values().GetValue(key) : 0.0f;
    if (!mProbe->Set(key, value, perr))
    {
        Copy(perr, err);
        if (!known) { CopyText(err.key, sizeof(err.key), dottedKey); }
        return false;
    }
    mProbe->Set(key, Json::Value(static_cast<double>(before)), perr);   // revert: a queued set is not applied yet
    return true;
}

bool WeatherParameterStore::Set(const char* dottedKey, const Json::Value& value, ParamError& err)
{
    err.Clear();
    if (!mLoaded)
    {
        err.Set(dottedKey, "parameters are not loaded");
        return false;
    }
    Dia::Attribute::ParamError perr;
    const Dia::Core::StringCRC key(dottedKey);
    if (!mFile->Set(key, value, perr))
    {
        Copy(perr, err);
        return false;
    }
    mProbe->Set(key, value, perr);   // keep the probe equal to the file
    return true;
}

bool WeatherParameterStore::ApplyPack(const Json::Value& pack, ParamError& err)
{
    err.Clear();
    if (!mLoaded)
    {
        err.Set("pack", "parameters are not loaded");
        return false;
    }
    Dia::Attribute::ParamError perr;
    ParamWarnings warn;
    if (!mFile->ApplyPack(pack, warn, perr))
    {
        Copy(perr, err);
        return false;
    }
    LogWarnings(warn);
    ParamWarnings ignored;
    mProbe->ApplyPack(pack, ignored, perr);
    return true;
}

bool WeatherParameterStore::Reload(ParamError& err)
{
    err.Clear();
    if (!mLoaded || mSource == nullptr)
    {
        err.Set("sidecar", "parameters are not loaded");
        return false;
    }
    std::string text, error;
    if (!mSource->Read(text, error))
    {
        err.Set("sidecar", error.c_str());
        return false;
    }
    std::vector<const Json::Value*> packs;
    for (const Json::Value& p : mPacks) { packs.push_back(&p); }
    const Json::Value* const* packPtr = packs.empty() ? nullptr : packs.data();
    const uint32_t packCount = static_cast<uint32_t>(packs.size());

    // The probe goes first: a bad sidecar, or a binding the new schema breaks, must not touch the live file.
    Dia::Attribute::ParamError perr;
    ParamWarnings warn;
    if (!mProbe->Reload(text.c_str(), packPtr, packCount, warn, perr))
    {
        Copy(perr, err);
        return false;
    }
    std::unique_ptr<Dia::Attribute::ParamBinding<WeatherParameters>> binding(new Dia::Attribute::ParamBinding<WeatherParameters>());
    if (!Bind(*binding, *mProbe, err))
    {
        mProbe->Reload(mText.c_str(), packPtr, packCount, warn, perr);   // back in step with the live file
        return false;
    }
    ParamWarnings liveWarn;
    if (!mFile->Reload(text.c_str(), packPtr, packCount, liveWarn, perr))
    {
        Copy(perr, err);
        mProbe->Reload(mText.c_str(), packPtr, packCount, warn, perr);
        return false;
    }
    LogWarnings(liveWarn);
    mBinding = std::move(binding);
    mText = text;
    return true;
}

} // namespace Weather
} // namespace CoW
