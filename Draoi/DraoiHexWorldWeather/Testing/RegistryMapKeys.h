// Test helper: an IMapKeySource that serves the biome ids of a BiomeRegistry for the keyRef "cow.biomes/biomes[].id".
// Ships with the library (no gtest dependency). The registry must outlive the key source and any ParameterFile
// loaded with it (ParameterFile keeps the pointer for Reload). Spec: weather-data.plan.md (Tasks 5-8).
#pragma once

#include <DiaAttribute/IMapKeySource.h>
#include <DraoiHexWorld/BiomeRegistry.h>

#include <cstring>
#include <vector>

namespace Draoi {
namespace HexWorld {
namespace Weather {
namespace Testing {

class RegistryMapKeySource final : public Dia::Attribute::IMapKeySource
{
public:
    static constexpr const char* kBiomeKeyRef = "cow.biomes/biomes[].id";

    explicit RegistryMapKeySource(const BiomeRegistry& registry)
    {
        for (uint32_t i = 0; i < registry.Count(); ++i)
        {
            mIds.push_back(registry.Id(static_cast<BiomeIndex>(i)));
            mNames.push_back(registry.IdText(static_cast<BiomeIndex>(i)));
        }
    }

    bool GetKeys(const char* keyRef, Dia::Attribute::MapKeys& out) const override
    {
        if (keyRef == nullptr || std::strcmp(keyRef, kBiomeKeyRef) != 0) return false;
        out.ids   = mIds.data();
        out.names = mNames.data();
        out.count = static_cast<uint32_t>(mIds.size());
        return true;
    }

private:
    std::vector<Dia::Core::StringCRC> mIds;
    std::vector<const char*>          mNames;
};

} // namespace Testing
} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
