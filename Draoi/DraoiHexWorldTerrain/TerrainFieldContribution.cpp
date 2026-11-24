#include <DraoiHexWorldTerrain/TerrainFieldContribution.h>

#include <DraoiHexWorld/CellVocabulary.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DraoiHexWorldTerrain/GuaranteeSpec.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>

#include <cctype>
#include <cstring>

namespace Draoi {
namespace HexWorld {

bool TerrainFieldSource::AddProtectedKind(uint32_t kindCrc)
{
    if (IsProtectedKind(kindCrc)) { return true; }
    if (mProtectedCount >= kMaxProtectedKinds) { return false; }
    mProtected[mProtectedCount++] = kindCrc;
    return true;
}

bool TerrainFieldSource::AddProtectedKindsFrom(const GuaranteeSpec& spec)
{
    for (const SiteKindDecl& k : spec.siteKinds)
    {
        if (k.isProtected && !AddProtectedKind(Dia::Core::StringCRC(k.id.c_str()).Value())) { return false; }
    }
    return true;
}

bool TerrainFieldSource::IsProtectedKind(uint32_t kindCrc) const
{
    for (uint32_t i = 0; i < mProtectedCount; ++i)
    {
        if (mProtected[i] == kindCrc) { return true; }
    }
    return false;
}

namespace
{
    // The committed cell, or nullptr until a generation exists / for a cell outside the world.
    const Cell* CellAt(const TerrainFieldSource& s, uint32_t cell)
    {
        const WorldGenSession& session = s.Session();
        if (session.Revision() == 0 || cell >= static_cast<uint32_t>(session.World().CellCount())) { return nullptr; }
        return &session.World().Cells()[cell];
    }

    float ReadLayer(const void* ctx, uint32_t cell)
    {
        const TerrainFieldSource::LayerBinding& b = *static_cast<const TerrainFieldSource::LayerBinding*>(ctx);
        if (CellAt(*b.owner, cell) == nullptr) { return 0.0f; }
        const TerrainLayers& layers = b.owner->Session().Generator().GetLayers();
        if (const float* f = layers.GetFloat(b.layer)) { return f[cell]; }
        if (const uint32_t* u = layers.GetUInt(b.layer)) { return static_cast<float>(u[cell]); }
        return 0.0f;
    }

    float ReadRegion(const void* ctx, uint32_t cell)
    {
        const Cell* c = CellAt(*static_cast<const TerrainFieldSource*>(ctx), cell);
        return c ? static_cast<float>(c->region) : 0.0f;
    }

    bool ReadIsRiverBool(const void* ctx, uint32_t cell)
    {
        const Cell* c = CellAt(*static_cast<const TerrainFieldSource*>(ctx), cell);
        return c != nullptr && c->waterSource == WaterSource::River;
    }
    float ReadIsRiverFloat(const void* ctx, uint32_t cell) { return ReadIsRiverBool(ctx, cell) ? 1.0f : 0.0f; }

    bool ReadIsProtectedBool(const void* ctx, uint32_t cell)
    {
        const TerrainFieldSource& s = *static_cast<const TerrainFieldSource*>(ctx);
        const Cell* c = CellAt(s, cell);
        if (c == nullptr) { return false; }
        if ((c->terrainFlags & (TerrainFlag::Pass | TerrainFlag::RiverCrossesBarrier)) != 0) { return true; }
        const WorldGenSession& session = s.Session();
        const uint32_t corridor = session.Registry().NodeKinds().corridor.Value();
        const SitesAtCell sites = session.World().sites.At(cell);
        for (uint32_t i = 0; i < sites.Size(); ++i)
        {
            const uint32_t kind = sites[i].kindCrc;
            if (kind == corridor || s.IsProtectedKind(kind)) { return true; }
        }
        return false;
    }
    float ReadIsProtectedFloat(const void* ctx, uint32_t cell) { return ReadIsProtectedBool(ctx, cell) ? 1.0f : 0.0f; }

    // elevationRaw -> elevation_raw
    void SnakeCase(const char* in, char* out, size_t cap)
    {
        size_t n = 0;
        for (; *in != '\0' && n + 2 < cap; ++in)
        {
            if (std::isupper(static_cast<unsigned char>(*in)))
            {
                if (n > 0) { out[n++] = '_'; }
                out[n++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*in)));
            }
            else { out[n++] = *in; }
        }
        out[n] = '\0';
    }
}

void RegisterTerrainFields(WorldCellFields& fields, TerrainFieldSource& source)
{
    const Dia::Core::StringCRC slot("terrain");

    const int layerCount = TerrainLayers::GetLayerCount();
    uint32_t bound = 0;
    for (int i = 0; i < layerCount && bound < TerrainFieldSource::kMaxLayers; ++i)
    {
        const char* name = TerrainLayers::GetLayerInfo(i).name;
        if (name == nullptr || std::strcmp(name, "biomeIndex") == 0) { continue; }   // biome.index owns it

        TerrainFieldSource::LayerBinding& b = source.Binding(bound++);
        b.owner = &source;
        b.layer = i;

        char field[64];
        SnakeCase(name, field, sizeof(field));
        FieldReader r{};
        r.getFloat = &ReadLayer;
        r.ctx      = &b;
        fields.Register(slot, Dia::Core::StringCRC(field), r);
    }

    FieldReader region{};
    region.getFloat = &ReadRegion;
    region.ctx      = &source;
    fields.Register(slot, Dia::Core::StringCRC("region"), region);

    FieldReader river{};
    river.getFloat = &ReadIsRiverFloat;
    river.getBool  = &ReadIsRiverBool;
    river.ctx      = &source;
    fields.Register(slot, Dia::Core::StringCRC("is_river"), river);

    FieldReader prot{};
    prot.getFloat = &ReadIsProtectedFloat;
    prot.getBool  = &ReadIsProtectedBool;
    prot.ctx      = &source;
    fields.Register(slot, Dia::Core::StringCRC("is_protected"), prot);
}

} // namespace HexWorld
} // namespace Draoi
