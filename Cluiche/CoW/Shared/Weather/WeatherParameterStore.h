// The weather parameter sidecar at run time: ParameterFile (strict load, packs, strict Set, Reload) + the
// ParamBinding<WeatherParameters> that snapshots it once per step. No compiled defaults and no fallback: a fault leaves
// the store unloaded and the caller fails the stage.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-module.md (Parameters; queue ACs 18-23)
#pragma once

#include <DiaAttribute/IMapKeySource.h>
#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorldWeather/WeatherParameters.h>

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

namespace Draoi { namespace HexWorld { class BiomeRegistry; } }

namespace CoW {
namespace Weather {

// A parameter fault, in fixed buffers (no allocation; safe to hold on the queue-API call path).
struct ParamError
{
    char key[128];
    char reason[256];
    ParamError() { Clear(); }
    void Clear() { key[0] = '\0'; reason[0] = '\0'; }
    void Set(const char* k, const char* r);
};

// Where the sidecar text comes from. The module owns a file source; tests inject a counting one (AC 23).
class IWeatherSidecarSource
{
public:
    virtual ~IWeatherSidecarSource() {}
    virtual bool Read(std::string& text, std::string& error) = 0;   // false + error when unreadable
};

class FileSidecarSource : public IWeatherSidecarSource
{
public:
    explicit FileSidecarSource(const std::string& path) : mPath(path) {}
    bool Read(std::string& text, std::string& error) override;
private:
    std::string mPath;
};

class WeatherParameterStore
{
public:
    WeatherParameterStore();
    ~WeatherParameterStore();

    // Packs applied (ApplyPack, in order) after every Load and Reload. Copied.
    void SetActivePacks(const std::vector<Json::Value>& packs) { mPacks = packs; }

    // Strict: read the source, load against the registry's biome ids, apply the packs, bind and resolve. Any fault
    // returns false, names the key in `err` and leaves the store unloaded. The source and registry are borrowed and
    // must outlive the store's use (Reload reads the source again).
    bool Load(IWeatherSidecarSource& source, const Draoi::HexWorld::BiomeRegistry& registry, ParamError& err);
    bool IsLoaded() const { return mLoaded; }

    void Snapshot(Draoi::HexWorld::Weather::WeatherParameters& out) const;
    uint64_t Hash() const;

    // Validates a set against the current values without changing them (the stage's queue-time check).
    bool CheckSet(const char* dottedKey, const Json::Value& value, ParamError& err);
    // Strict, atomic. Applied values are visible to the next Snapshot.
    bool Set(const char* dottedKey, const Json::Value& value, ParamError& err);
    // Lenient pack, atomic (ParameterFile::ApplyPack).
    bool ApplyPack(const Json::Value& pack, ParamError& err);
    // Re-reads the source and reloads with the active packs. A failure changes nothing (previous values stay).
    bool Reload(ParamError& err);

private:
    class RegistryKeys : public Dia::Attribute::IMapKeySource
    {
    public:
        void Build(const Draoi::HexWorld::BiomeRegistry& registry);
        bool GetKeys(const char* keyRef, Dia::Attribute::MapKeys& out) const override;
    private:
        std::vector<Dia::Core::StringCRC> mIds;
        std::vector<std::string> mNames;
        std::vector<const char*> mNamePtrs;
    };

    static void Copy(const Dia::Attribute::ParamError& from, ParamError& to);
    bool Bind(Dia::Attribute::ParamBinding<Draoi::HexWorld::Weather::WeatherParameters>& binding,
              const Dia::Attribute::ParameterFile& file, ParamError& err) const;
    bool LoadInto(Dia::Attribute::ParameterFile& file, const std::string& text, ParamError& err);

    RegistryKeys mKeys;   // declared before the files: they keep a pointer to it
    std::unique_ptr<Dia::Attribute::ParameterFile> mFile;
    std::unique_ptr<Dia::Attribute::ParameterFile> mProbe;   // mirrors mFile; CheckSet sets and reverts on it
    std::unique_ptr<Dia::Attribute::ParamBinding<Draoi::HexWorld::Weather::WeatherParameters>> mBinding;
    std::string mText;   // the sidecar text the live file was last loaded from
    IWeatherSidecarSource* mSource = nullptr;
    const Draoi::HexWorld::BiomeRegistry* mRegistry = nullptr;
    std::vector<Json::Value> mPacks;
    bool mLoaded = false;
};

} // namespace Weather
} // namespace CoW
