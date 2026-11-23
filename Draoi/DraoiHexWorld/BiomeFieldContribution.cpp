#include <DraoiHexWorld/BiomeFieldContribution.h>

#include <cstdio>

namespace Draoi {
namespace HexWorld {

namespace
{
    // The registry's tag vocabulary (BiomeRegistry.cpp kTagNames). The registry exposes only FindTag, not its list, so the
    // names are repeated here; a name the registry no longer knows is skipped (FindTag), a new registry tag needs adding here.
    const char* const kTagNames[] = { "land", "water", "aquatic", "wet", "cold", "hot",
                                      "forest", "open", "fuel", "mountain", "volcanic", "succession" };

    BiomeIndex BiomeAt(const BiomeFieldSource& s, uint32_t cell)
    {
        const WorldStructure& w = s.World();
        return cell < static_cast<uint32_t>(w.CellCount()) ? w.Cells()[cell].biome : kNoBiome;
    }

    float ReadIndex(const void* ctx, uint32_t cell)
    {
        const BiomeIndex b = BiomeAt(*static_cast<const BiomeFieldSource*>(ctx), cell);
        return b == kNoBiome ? 0.0f : static_cast<float>(b);
    }

    bool ReadTagBool(const void* ctx, uint32_t cell)
    {
        const BiomeFieldSource::TagBinding& t = *static_cast<const BiomeFieldSource::TagBinding*>(ctx);
        const BiomeIndex b = BiomeAt(*t.owner, cell);
        return b != kNoBiome && t.owner->Biomes().HasTag(b, t.tag);
    }

    float ReadTagFloat(const void* ctx, uint32_t cell) { return ReadTagBool(ctx, cell) ? 1.0f : 0.0f; }
}

void RegisterBiomeFields(WorldCellFields& fields, BiomeFieldSource& source)
{
    const Dia::Core::StringCRC slot("biome");

    FieldReader index{};
    index.getFloat = &ReadIndex;
    index.ctx      = &source;
    fields.Register(slot, Dia::Core::StringCRC("index"), index);

    uint32_t bound = 0;
    for (const char* tagName : kTagNames)
    {
        const TagId tag = source.Biomes().FindTag(Dia::Core::StringCRC(tagName));
        if (tag == kNoTag || bound >= BiomeFieldSource::kMaxTags) { continue; }

        BiomeFieldSource::TagBinding& b = source.Binding(bound++);
        b.owner = &source;
        b.tag   = tag;

        char name[96];
        std::snprintf(name, sizeof(name), "tag.%s", tagName);
        FieldReader r{};
        r.getFloat = &ReadTagFloat;
        r.getBool  = &ReadTagBool;
        r.ctx      = &b;
        fields.Register(slot, Dia::Core::StringCRC(name), r);
    }
}

} // namespace HexWorld
} // namespace Draoi
