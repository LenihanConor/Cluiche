#include <gtest/gtest.h>
#include <DraoiHexWorld/HexWorldSaveCodec.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/Logger.h>

#include <DiaCore/Json/external/json/json.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

using namespace Draoi::HexWorld;

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    // Fields: "elev" F32 PerCell [-10, 10]; "kind" U8 PerCell [0, 255]; "idx" U16 PerBasin [0, 65535];
    //         "dir" F32 PerCell angular [0, 2pi].
    const SavedField kFields[] = {
        { "elev", SavedKind::F32, SavedScope::PerCell,  -10.0, 10.0 },
        { "kind", SavedKind::U8,  SavedScope::PerCell,  0.0, 255.0 },
        { "idx",  SavedKind::U16, SavedScope::PerBasin, 0.0, 65535.0 },
        { "dir",  SavedKind::F32, SavedScope::PerCell,  0.0, kTwoPi },
    };
    constexpr uint32_t kFieldCount = 4;

    Caps MakeCaps() { Caps c; c.maxBasins = 8; return c; }

    bool Parse(const char* text, Json::Value& out)
    {
        Json::CharReaderBuilder b;
        std::unique_ptr<Json::CharReader> r(b.newCharReader());
        std::string errs;
        return r->parse(text, text + std::strlen(text), &out, &errs);
    }

    // Valid 2x1 world, 2 cells, 3 basins.
    const char* kGood =
        R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":123,"basin_count":3,"world_hash":456,)"
        R"("elev":[1.5,-2.5],"kind":[0,255],"idx":[0,1,65535],"dir":[0.0,6.283185307179586]})";

    bool ValidateText(const char* text, SaveError& err, const SavedField* f = kFields, const Caps& caps = MakeCaps())
    {
        Json::Value v;
        EXPECT_TRUE(Parse(text, v));
        return Validate(v, f, kFieldCount, caps, err);
    }

    struct Fault { const char* label; const char* json; const char* field; };
}

TEST(HexWorldSaveCodec, GoodRecordAccepted)
{
    SaveError err;
    EXPECT_TRUE(ValidateText(kGood, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
}

TEST(HexWorldSaveCodec, ReadyFalseOnlyReadyAccepted)
{
    SaveError err;
    EXPECT_TRUE(ValidateText(R"({"ready":false})", err));
}

TEST(HexWorldSaveCodec, ReadyMissingRejected)
{
    SaveError err;
    EXPECT_FALSE(ValidateText(R"({})", err));
    EXPECT_STREQ("ready", err.field.AsCStr());
}

TEST(HexWorldSaveCodec, WriteHeaderKeys)
{
    SaveHeader h;
    h.width = 4; h.height = 3; h.cellCount = 12; h.stepDay = 7; h.paramsHash = 99; h.basinCount = 2; h.worldHash = 1234;
    Json::Value root;
    WriteHeader(root, h);
    EXPECT_TRUE(root["ready"].asBool());
    EXPECT_EQ(4u, root["width"].asUInt());
    EXPECT_EQ(3u, root["height"].asUInt());
    EXPECT_EQ(12u, root["cell_count"].asUInt());
    EXPECT_EQ(7, root["step_day"].asInt());
    EXPECT_EQ(99u, root["params_hash"].asUInt64());
    EXPECT_EQ(2u, root["basin_count"].asUInt());
    EXPECT_EQ(1234u, root["world_hash"].asUInt64());
}

TEST(HexWorldSaveCodec, WriteNotReadyHoldsOnlyReady)
{
    Json::Value root;
    WriteNotReady(root);
    EXPECT_FALSE(root["ready"].asBool());
    EXPECT_EQ(1u, root.size());
}

TEST(HexWorldSaveCodec, ParamsHashFullRangeRoundTrips)
{
    SaveHeader h;
    h.width = 1; h.height = 1; h.cellCount = 1; h.stepDay = 0; h.basinCount = 0; h.worldHash = 0xFFFFFFFFFFFFFFFFull;
    h.paramsHash = 0xFFFFFFFFFFFFFFFEull;
    Json::Value root;
    WriteHeader(root, h);
    Json::Value back;
    ASSERT_TRUE(Parse(Serialize(root).c_str(), back));
    EXPECT_EQ(0xFFFFFFFFFFFFFFFEull, back["params_hash"].asUInt64());
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFull, back["world_hash"].asUInt64());
}

TEST(HexWorldSaveCodec, SeventeenDigitRoundTrip)
{
    // 0.1f as a double is 0.10000000149011612 (17 sig digits needed); must survive text and back exactly.
    const float vals[3] = { 0.1f, -3.3333333f, 1.0f };
    Json::Value root;
    WriteArray(root, "elev", vals, 3);
    const std::string text = Serialize(root);
    EXPECT_NE(std::string::npos, text.find("0.10000000149011612"));
    Json::Value back;
    ASSERT_TRUE(Parse(text.c_str(), back));
    ASSERT_EQ(3u, back["elev"].size());
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(static_cast<double>(vals[i]), back["elev"][i].asDouble());
}

TEST(HexWorldSaveCodec, NegativeZeroWrittenAsPositiveZero)
{
    const float vals[2] = { -0.0f, 2.0f };
    Json::Value root;
    WriteArray(root, "elev", vals, 2);
    EXPECT_FALSE(std::signbit(root["elev"][0].asDouble()));
    EXPECT_EQ(std::string::npos, Serialize(root).find("-0"));
}

TEST(HexWorldSaveCodec, WriteIsDeterministic)
{
    const float vals[2] = { 0.25f, -0.0f };
    Json::Value a, b;
    WriteArray(a, "elev", vals, 2);
    WriteArray(b, "elev", vals, 2);
    const std::string sa = Serialize(a), sb = Serialize(b);
    ASSERT_EQ(sa.size(), sb.size());
    EXPECT_EQ(0, std::memcmp(sa.data(), sb.data(), sa.size()));
}

TEST(HexWorldSaveCodec, WrittenRecordValidates)
{
    SaveHeader h;
    h.width = 2; h.height = 1; h.cellCount = 2; h.stepDay = 0; h.paramsHash = 1; h.basinCount = 1; h.worldHash = 2;
    Json::Value root;
    WriteHeader(root, h);
    const float elev[2] = { 1.0f, -1.0f };
    const float kind[2] = { 3.0f, 4.0f };
    const float idx[1]  = { 9.0f };
    const float dir[2]  = { 0.0f, 3.0f };
    WriteArray(root, "elev", elev, 2);
    WriteArray(root, "kind", kind, 2);
    WriteArray(root, "idx", idx, 1);
    WriteArray(root, "dir", dir, 2);
    Json::Value back;
    ASSERT_TRUE(Parse(Serialize(root).c_str(), back));
    SaveError err;
    EXPECT_TRUE(Validate(back, kFields, kFieldCount, MakeCaps(), err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
}

TEST(HexWorldSaveCodec, TableDrivenFaults)
{
    static const Fault faults[] = {
        { "missing width",
          R"({"ready":true,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "width" },
        { "missing step_day",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "step_day" },
        { "missing array",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"idx":[0,1,2],"dir":[0,1]})",
          "kind" },
        // 3*1 = 3 != 2
        { "cell_count mismatch",
          R"({"ready":true,"width":3,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "cell_count" },
        { "cell_count zero",
          R"({"ready":true,"width":0,"height":5,"cell_count":0,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[],"kind":[],"idx":[0,1,2],"dir":[]})",
          "cell_count" },
        // 65537*1 matches width*height but is over the 65,536 cap
        { "cell_count 65537",
          R"({"ready":true,"width":65537,"height":1,"cell_count":65537,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[],"kind":[],"idx":[0,1,2],"dir":[]})",
          "cell_count" },
        { "basin cap exceeded (9 > 8)",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":9,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2,3,4,5,6,7,8],"dir":[0,1]})",
          "basin_count" },
        { "negative step_day",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":-1,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "step_day" },
        { "per-cell array too short",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "elev" },
        { "per-basin array too long",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2,3],"dir":[0,1]})",
          "idx" },
        { "array not an array",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":5,"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "elev" },
        { "element not a number",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,"x"],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "elev" },
        { "out of range above",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,10.5],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "elev" },
        { "out of range below",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[-10.5,1],"kind":[0,1],"idx":[0,1,2],"dir":[0,1]})",
          "elev" },
        { "U8 overflow 256",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,256],"idx":[0,1,2],"dir":[0,1]})",
          "kind" },
        { "U8 non-integral",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1.5],"idx":[0,1,2],"dir":[0,1]})",
          "kind" },
        { "U16 overflow 65536",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,65536],"dir":[0,1]})",
          "idx" },
        { "U16 negative",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,-1],"dir":[0,1]})",
          "idx" },
        // 2pi = 6.283185307179586; 6.2831853071795871 is the next double up
        { "angular above 2pi",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2],"dir":[0,6.2831853071795871]})",
          "dir" },
        { "angular negative",
          R"({"ready":true,"width":2,"height":1,"cell_count":2,"step_day":5,"params_hash":1,"basin_count":3,"world_hash":4,"elev":[1,2],"kind":[0,1],"idx":[0,1,2],"dir":[-0.1,1]})",
          "dir" },
    };

    for (const Fault& f : faults)
    {
        SaveError err;
        EXPECT_FALSE(ValidateText(f.json, err)) << f.label;
        EXPECT_STREQ(f.field, err.field.AsCStr()) << f.label << " reason: " << err.reason.AsCStr();
        EXPECT_GT(std::strlen(err.reason.AsCStr()), 0u) << f.label;
    }
}

TEST(HexWorldSaveCodec, NonFiniteRejectedNamingField)
{
    const double bad[3] = { std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity() };
    for (double b : bad)
    {
        Json::Value v;
        ASSERT_TRUE(Parse(kGood, v));
        v["elev"][1] = Json::Value(b);
        SaveError err;
        EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
        EXPECT_STREQ("elev", err.field.AsCStr());
    }
}

TEST(HexWorldSaveCodec, CallerRangeFlipsVerdict)
{
    // elev holds 1.5 and -2.5. Under [-10,10] accepted; under [-2,10] the -2.5 is rejected; no code change.
    SaveError err;
    EXPECT_TRUE(ValidateText(kGood, err));

    SavedField narrow[kFieldCount];
    for (uint32_t i = 0; i < kFieldCount; ++i) narrow[i] = kFields[i];
    narrow[0].min = -2.0;
    EXPECT_FALSE(ValidateText(kGood, err, narrow));
    EXPECT_STREQ("elev", err.field.AsCStr());
}

TEST(HexWorldSaveCodec, AngularTwoPiAcceptedAboveRejected)
{
    // kGood already holds exactly 2pi in "dir" and is accepted.
    SaveError err;
    EXPECT_TRUE(ValidateText(kGood, err));
    Json::Value v;
    ASSERT_TRUE(Parse(kGood, v));
    v["dir"][1] = Json::Value(kTwoPi + 1e-9);
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("dir", err.field.AsCStr());
}

TEST(HexWorldSaveCodec, MaxCellsAccepted)
{
    // 256 x 256 = 65,536 cells, one U8 field.
    const SavedField one[] = { { "kind", SavedKind::U8, SavedScope::PerCell, 0.0, 255.0 } };
    Json::Value v;
    v["ready"] = true; v["width"] = 256; v["height"] = 256; v["cell_count"] = 65536; v["step_day"] = 0;
    v["params_hash"] = Json::UInt64(1); v["basin_count"] = 0; v["world_hash"] = Json::UInt64(2);
    for (int i = 0; i < 65536; ++i) v["kind"].append(0.0);
    SaveError err;
    EXPECT_TRUE(Validate(v, one, 1, MakeCaps(), err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
}

TEST(HexWorldSaveCodec, ValidatePendingRunsSameChecks)
{
    Json::Value v;
    ASSERT_TRUE(Parse(kGood, v));
    SaveError err;
    EXPECT_TRUE(ValidatePending(v, kFields, kFieldCount, MakeCaps(), err));
    v["elev"][0] = Json::Value(99.0);
    EXPECT_FALSE(ValidatePending(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("elev", err.field.AsCStr());
}

// ---- Biome palette (D6, AC 21) ----

namespace
{
    // Rows copied from the BiomeRegistry fixture. Registry order is the order passed to BuildRegistryText.
    const char* const kRowOcean =
        R"({"id":"Biome.Ocean","name":"Ocean","colour":"#1f4f8f","description":"Open sea","tags":["water","aquatic"],)"
        R"("generated":true,"zones":["None"],"roles":["ocean"],"pin":{"rule":"water_ocean","priority":100}})";
    const char* const kRowLake =
        R"({"id":"Biome.Lake","name":"Lake","colour":"#3a7fd0","description":"Inland water","tags":["water","aquatic"],)"
        R"("generated":true,"zones":["None"],"roles":["lake"],"pin":{"rule":"water_lake","priority":200}})";
    const char* const kRowGrass =
        R"({"id":"Biome.Grassland","name":"Grassland","colour":"#9acd32","description":"Open plains","tags":["land","open","fuel"],)"
        R"("generated":true,"zones":["FirstShores","WildReaches"],"roles":["fallback"],)"
        R"("climate":{"t0":0.5,"m0":0.25,"wt":0.125,"wm":0.75},"pin":{"rule":"wetland","priority":150},"plateau":"dry"})";
    const char* const kRowScrub =
        R"({"id":"Biome.Scrub","name":"Scrub","colour":"#808000","description":"Succession scrub","tags":["land","succession"],)"
        R"("generated":false,"zones":[],)"
        R"("climate":{"t0":0.75,"m0":0.5,"wt":0.25,"wm":0.375},"plateau":"wet"})";
    const char* const kRowTundra =
        R"({"id":"Biome.Tundra","name":"Tundra","colour":"#aabbcc","description":"Cold plains","tags":["land"],)"
        R"("generated":false,"zones":[]})";

    std::string BuildRegistryText(std::initializer_list<const char*> rows)
    {
        std::string t = R"({"format":"cow.biomes","version":1,"biomes":[)";
        bool first = true;
        for (const char* r : rows) { if (!first) t += ","; t += r; first = false; }
        t += R"(],"forbidden":[]})";
        return t;
    }

    void LoadRegistry(BiomeRegistry& r, const std::string& text)
    {
        RegistryError err;
        ASSERT_TRUE(r.LoadFromText(text.c_str(), ::Draoi::HexWorld::Testing::ShippedStratumTable(), err)) << err.reason.AsCStr();
    }

    // Counts the unknown-id warnings (BiomeRegistry) and the palette-hash Info (codec) by message marker.
    class PaletteSink : public Dia::Observation::Log::ISink
    {
    public:
        void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
        {
            const std::string m(e.message);
            if (m.find("BiomeRegistry unknown id") != std::string::npos) { ++warnings; }
            if (m.find("biome palette hash differs") != std::string::npos) { ++infos; }
        }
        const char* GetName() const override { return "HexWorldPaletteSink"; }
        unsigned int warnings = 0;
        unsigned int infos = 0;
    };

    class HexWorldSaveCodecPalette : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // A = Ocean, Lake, Grassland, Scrub (indices 0..3). B = A + Tundra. C = Scrub, Ocean, Grassland, Lake.
            // D = A without Scrub.
            LoadRegistry(a, BuildRegistryText({ kRowOcean, kRowLake, kRowGrass, kRowScrub }));
            LoadRegistry(b, BuildRegistryText({ kRowOcean, kRowLake, kRowGrass, kRowScrub, kRowTundra }));
            LoadRegistry(c, BuildRegistryText({ kRowScrub, kRowOcean, kRowGrass, kRowLake }));
            LoadRegistry(d, BuildRegistryText({ kRowOcean, kRowLake, kRowGrass }));
            auto& l = Dia::Observation::Log::Logger::Instance();
            l.RegisterThreadBuffer();
            l.RegisterSink(&sink);
        }
        void TearDown() override
        {
            auto& l = Dia::Observation::Log::Logger::Instance();
            l.FlushSync();
            l.UnregisterSink(&sink);
            l.UnregisterThreadBuffer();
        }
        void Flush() { Dia::Observation::Log::Logger::Instance().FlushSync(); }

        // Under A: cells Scrub, Ocean, Scrub, Lake, Grassland = indices 3,0,3,1,2.
        Json::Value WrittenUnderA()
        {
            const BiomeIndex cells[5] = { 3, 0, 3, 1, 2 };
            Json::Value root;
            WriteBiomeCells(root, a, cells, 5);
            Json::Value back;
            EXPECT_TRUE(Parse(Serialize(root).c_str(), back));
            return back;
        }

        BiomeRegistry a, b, c, d;
        PaletteSink sink;
    };
}

TEST_F(HexWorldSaveCodecPalette, PaletteWrittenOnceInRegistryOrder)
{
    const BiomeIndex cells[5] = { 3, 0, 3, 1, 2 };
    Json::Value root;
    WriteBiomeCells(root, a, cells, 5);
    const std::string text = Serialize(root);
    // One palette key in the text: first find == last find.
    EXPECT_NE(std::string::npos, text.find("\"biome_palette\":"));
    EXPECT_EQ(text.find("\"biome_palette\":"), text.rfind("\"biome_palette\":"));
    ASSERT_EQ(4u, root["biome_palette"].size());
    EXPECT_EQ("Biome.Ocean", root["biome_palette"][0].asString());
    EXPECT_EQ("Biome.Lake", root["biome_palette"][1].asString());
    EXPECT_EQ("Biome.Grassland", root["biome_palette"][2].asString());
    EXPECT_EQ("Biome.Scrub", root["biome_palette"][3].asString());
    ASSERT_EQ(5u, root["biome"].size());
    const unsigned expected[5] = { 3, 0, 3, 1, 2 };
    for (uint32_t i = 0; i < 5; ++i) { EXPECT_EQ(expected[i], root["biome"][i].asUInt()); }
}

TEST_F(HexWorldSaveCodecPalette, HashStoredEqualsRegistryHash)
{
    Json::Value root = WrittenUnderA();
    EXPECT_NE(0ull, a.Hash());
    EXPECT_EQ(a.Hash(), StoredBiomeHash(root));
}

TEST_F(HexWorldSaveCodecPalette, RoundTripUnderSameRegistry)
{
    Json::Value root = WrittenUnderA();
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, a, out, 5, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    const BiomeIndex expected[5] = { 3, 0, 3, 1, 2 };
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(expected[i], out[i]); }
    Flush();
    EXPECT_EQ(0u, sink.infos);
    EXPECT_EQ(0u, sink.warnings);
}

TEST_F(HexWorldSaveCodecPalette, LoadUnderAppendedRegistrySameIds)
{
    // B appends Tundra at index 4; existing indices unchanged: 3,0,3,1,2 still Scrub,Ocean,Scrub,Lake,Grassland.
    Json::Value root = WrittenUnderA();
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, b, out, 5, err));
    const char* ids[5] = { "Biome.Scrub", "Biome.Ocean", "Biome.Scrub", "Biome.Lake", "Biome.Grassland" };
    for (int i = 0; i < 5; ++i) { EXPECT_STREQ(ids[i], b.IdText(out[i])); }
}

TEST_F(HexWorldSaveCodecPalette, LoadUnderReorderedRegistrySameIds)
{
    // C order: 0 Scrub, 1 Ocean, 2 Grassland, 3 Lake. Cells Scrub,Ocean,Scrub,Lake,Grassland -> 0,1,0,3,2.
    Json::Value root = WrittenUnderA();
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, c, out, 5, err));
    const BiomeIndex expected[5] = { 0, 1, 0, 3, 2 };
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(expected[i], out[i]); }
    const char* ids[5] = { "Biome.Scrub", "Biome.Ocean", "Biome.Scrub", "Biome.Lake", "Biome.Grassland" };
    for (int i = 0; i < 5; ++i) { EXPECT_STREQ(ids[i], c.IdText(out[i])); }
}

TEST_F(HexWorldSaveCodecPalette, UnknownIdsLoadFallbackOneWarningPerId)
{
    // D lacks Scrub (used by 2 cells); Lake is renamed in the record to an unknown id (used by 1 cell).
    Json::Value root = WrittenUnderA();
    root["biome_palette"][1] = "Biome.Gone";
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, d, out, 5, err));
    // D: 0 Ocean, 1 Lake, 2 Grassland(Fallback). Cells: Scrub->2, Ocean->0, Scrub->2, Gone->2, Grassland->2.
    const BiomeIndex expected[5] = { 2, 0, 2, 2, 2 };
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(expected[i], out[i]); }
    EXPECT_EQ(d.Role(BiomeRole::Fallback), out[0]);
    Flush();
    EXPECT_EQ(2u, sink.warnings);   // Scrub once, Gone once
}

TEST_F(HexWorldSaveCodecPalette, SingleUnknownIdManyCellsOneWarning)
{
    const BiomeIndex cells[6] = { 3, 3, 3, 3, 3, 3 };
    Json::Value root;
    WriteBiomeCells(root, a, cells, 6);
    BiomeIndex out[6] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, d, out, 6, err));
    for (int i = 0; i < 6; ++i) { EXPECT_EQ(2, out[i]); }
    Flush();
    EXPECT_EQ(1u, sink.warnings);
}

TEST_F(HexWorldSaveCodecPalette, HashMismatchCleanRemapLogsExactlyOneInfo)
{
    Json::Value root = WrittenUnderA();
    ASSERT_NE(a.Hash(), b.Hash());
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, b, out, 5, err));
    Flush();
    EXPECT_EQ(1u, sink.infos);
    EXPECT_EQ(0u, sink.warnings);
}

TEST_F(HexWorldSaveCodecPalette, MalformedPaletteRejectedNamingField)
{
    SaveError err;
    BiomeIndex out[5] = {};

    Json::Value r = WrittenUnderA();
    r.removeMember("biome_palette");
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome_palette", err.field.AsCStr());

    r = WrittenUnderA();
    r["biome_palette"] = 7;   // not an array
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome_palette", err.field.AsCStr());

    r = WrittenUnderA();
    r["biome_palette"][2] = 5;   // entry not a string
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome_palette", err.field.AsCStr());

    r = WrittenUnderA();
    r["biome"][1] = 4;   // palette has 4 entries: index 4 is out of range
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome", err.field.AsCStr());

    r = WrittenUnderA();
    r["biome"][1] = 1.5;   // not an integer
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome", err.field.AsCStr());

    r = WrittenUnderA();   // 5 cells stored, caller expects 6
    BiomeIndex out6[6] = {};
    EXPECT_FALSE(ReadBiomeCells(r, a, out6, 6, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    EXPECT_GT(std::strlen(err.reason.AsCStr()), 0u);
}

// ---- D12 edge tests ----

namespace
{
    // Builds a record like kGood but with a caller-chosen header, so one key can be broken at a time.
    Json::Value GoodRecord()
    {
        Json::Value v;
        EXPECT_TRUE(Parse(kGood, v));
        return v;
    }
}

TEST(HexWorldSaveCodec, CellCountCapBoundaryAcceptsMaxRejectsOneOver)
{
    // 65,536 cells (65536 x 1) accepted; 65,537 (1 x 65537) rejected naming cell_count, with full-length arrays present so
    // only the cap can be the cause.
    const SavedField one[] = { { "kind", SavedKind::U8, SavedScope::PerCell, 0.0, 255.0 } };
    for (uint32_t cells : { kMaxSaveCells, kMaxSaveCells + 1u })
    {
        Json::Value v;
        v["ready"] = true; v["width"] = Json::UInt(cells); v["height"] = 1; v["cell_count"] = Json::UInt(cells); v["step_day"] = 0;
        v["params_hash"] = Json::UInt64(1); v["basin_count"] = 0; v["world_hash"] = Json::UInt64(2);
        v["kind"] = Json::Value(Json::arrayValue);
        for (uint32_t i = 0; i < cells; ++i) v["kind"].append(0.0);
        SaveError err;
        const bool ok = Validate(v, one, 1, MakeCaps(), err);
        if (cells == kMaxSaveCells)
            EXPECT_TRUE(ok) << err.field.AsCStr() << ": " << err.reason.AsCStr();
        else
        {
            EXPECT_FALSE(ok);
            EXPECT_STREQ("cell_count", err.field.AsCStr());
        }
    }
}

TEST(HexWorldSaveCodec, HugeWidthHeightProductDoesNotOverflowIntoAcceptance)
{
    Json::Value v = GoodRecord();
    v["width"] = Json::UInt(0xFFFFFFFFu);
    v["height"] = Json::UInt(0xFFFFFFFFu);   // product needs 64 bits; cell_count is 2
    SaveError err;
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("cell_count", err.field.AsCStr());
}

TEST(HexWorldSaveCodec, MalformedRootAndReadyRejectedNamingReady)
{
    SaveError err;
    Json::Value arr(Json::arrayValue);
    EXPECT_FALSE(Validate(arr, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("ready", err.field.AsCStr());
    Json::Value nul;
    EXPECT_FALSE(Validate(nul, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("ready", err.field.AsCStr());
    Json::Value v = GoodRecord();
    v["ready"] = 1;   // number, not a boolean
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("ready", err.field.AsCStr());
    v["ready"] = "true";
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("ready", err.field.AsCStr());
}

TEST(HexWorldSaveCodec, HeaderIntegerKeysRejectWrongTypesNamingKey)
{
    const char* keys[] = { "width", "height", "cell_count", "params_hash", "basin_count", "world_hash" };
    for (const char* key : keys)
    {
        const Json::Value bad[3] = { Json::Value(-1), Json::Value(1.5), Json::Value("7") };
        for (const Json::Value& b : bad)
        {
            Json::Value v = GoodRecord();
            v[key] = b;
            SaveError err;
            EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err)) << key << " " << Serialize(b);
            EXPECT_STREQ(key, err.field.AsCStr()) << key << " " << Serialize(b);
        }
    }
}

TEST(HexWorldSaveCodec, StepDayBoundaryAndWrongTypes)
{
    SaveError err;
    Json::Value v = GoodRecord();
    v["step_day"] = 0;
    EXPECT_TRUE(Validate(v, kFields, kFieldCount, MakeCaps(), err)) << err.reason.AsCStr();   // zero is not negative
    v["step_day"] = Json::Value(Json::UInt64(2147483647u));
    EXPECT_TRUE(Validate(v, kFields, kFieldCount, MakeCaps(), err)) << err.reason.AsCStr();   // INT32_MAX is the largest WriteHeader can write
    v["step_day"] = Json::Value(Json::UInt64(2147483648u));
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("step_day", err.field.AsCStr());
    v["step_day"] = 1.5;
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("step_day", err.field.AsCStr());
    v["step_day"] = "5";
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("step_day", err.field.AsCStr());
    v["step_day"] = true;
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("step_day", err.field.AsCStr());
}

TEST(HexWorldSaveCodec, BasinCapBoundary)
{
    // maxBasins = 8: exactly 8 accepted (8 per-basin entries), 9 rejected naming basin_count.
    SaveError err;
    Json::Value v = GoodRecord();
    v["basin_count"] = 8;
    v["idx"] = Json::Value(Json::arrayValue);
    for (int i = 0; i < 8; ++i) v["idx"].append(i);
    EXPECT_TRUE(Validate(v, kFields, kFieldCount, MakeCaps(), err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();

    // Zero basins with an empty per-basin array is valid.
    v["basin_count"] = 0;
    v["idx"] = Json::Value(Json::arrayValue);
    EXPECT_TRUE(Validate(v, kFields, kFieldCount, MakeCaps(), err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();

    // The cap is the caller's: the same 9-basin record passes under maxBasins = 9.
    v["basin_count"] = 9;
    v["idx"] = Json::Value(Json::arrayValue);
    for (int i = 0; i < 9; ++i) v["idx"].append(i);
    EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("basin_count", err.field.AsCStr());
    Caps nine; nine.maxBasins = 9;
    EXPECT_TRUE(Validate(v, kFields, kFieldCount, nine, err));
}

TEST(HexWorldSaveCodec, NonFiniteRejectedInIntegerKindFields)
{
    const double bad[3] = { std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity() };
    for (const char* name : { "kind", "idx", "dir" })
    {
        for (double b : bad)
        {
            Json::Value v = GoodRecord();
            v[name][0] = Json::Value(b);
            SaveError err;
            EXPECT_FALSE(Validate(v, kFields, kFieldCount, MakeCaps(), err)) << name;
            EXPECT_STREQ(name, err.field.AsCStr());
        }
    }
}

TEST(HexWorldSaveCodec, IntegerKindExactBoundsAcceptedAndRangeIntersectsKind)
{
    SaveError err;
    Json::Value v = GoodRecord();   // kind [0,255] and idx [..., 65535] already sit on the kind maxima
    EXPECT_TRUE(Validate(v, kFields, kFieldCount, MakeCaps(), err));
    // A caller range wider than the kind still rejects values beyond the kind (kind and range both apply).
    SavedField wide[kFieldCount];
    for (uint32_t i = 0; i < kFieldCount; ++i) wide[i] = kFields[i];
    wide[1].max = 1000.0;
    v["kind"][1] = 256;
    EXPECT_FALSE(Validate(v, wide, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("kind", err.field.AsCStr());
    // A caller range narrower than the kind rejects values inside the kind.
    wide[1].max = 100.0;
    v["kind"][1] = 101;
    EXPECT_FALSE(Validate(v, wide, kFieldCount, MakeCaps(), err));
    EXPECT_STREQ("kind", err.field.AsCStr());
    v["kind"][1] = 100;
    EXPECT_TRUE(Validate(v, wide, kFieldCount, MakeCaps(), err));
}

TEST(HexWorldSaveCodec, ZeroDeclaredFieldsValidatesHeaderOnly)
{
    Json::Value v = GoodRecord();
    SaveError err;
    EXPECT_TRUE(Validate(v, nullptr, 0, MakeCaps(), err));
}

// ---- Biome palette edge cases (D12) ----

TEST_F(HexWorldSaveCodecPalette, DuplicatePaletteIdsMapEachEntryIndependentlyToTheSameBiome)
{
    // Defined behaviour: the palette is a position -> id table; Read remaps every position through the registry, so two
    // positions naming the same id both resolve to that id's index (no error, no dedupe, no extra warning).
    // Write never produces duplicates (registry ids are unique); a hand-edited or merged record may.
    Json::Value root = WrittenUnderA();
    root["biome_palette"][1] = "Biome.Ocean";   // palette: Ocean, Ocean, Grassland, Scrub
    const unsigned stored[5] = { 0, 1, 2, 3, 1 };
    for (uint32_t i = 0; i < 5; ++i) root["biome"][i] = stored[i];
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, a, out, 5, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    const char* ids[5] = { "Biome.Ocean", "Biome.Ocean", "Biome.Grassland", "Biome.Scrub", "Biome.Ocean" };
    for (int i = 0; i < 5; ++i) { EXPECT_STREQ(ids[i], a.IdText(out[i])); }
    EXPECT_EQ(out[0], out[1]);
    Flush();
    EXPECT_EQ(0u, sink.warnings);
}

TEST_F(HexWorldSaveCodecPalette, DuplicateUnknownPaletteIdWarnsOnce)
{
    Json::Value root = WrittenUnderA();
    root["biome_palette"][0] = "Biome.Gone";
    root["biome_palette"][1] = "Biome.Gone";   // two positions, one distinct unknown id
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, a, out, 5, err));
    EXPECT_EQ(out[1], a.Role(BiomeRole::Fallback));   // stored 0 (Gone) -> fallback
    Flush();
    EXPECT_EQ(1u, sink.warnings);
}

TEST_F(HexWorldSaveCodecPalette, ZeroCellsWriteAndReadRoundTrip)
{
    Json::Value root;
    WriteBiomeCells(root, a, nullptr, 0);
    EXPECT_EQ(4u, root["biome_palette"].size());
    EXPECT_TRUE(root["biome"].isArray());
    EXPECT_EQ(0u, root["biome"].size());
    Json::Value back;
    ASSERT_TRUE(Parse(Serialize(root).c_str(), back));
    SaveError err;
    EXPECT_TRUE(ReadBiomeCells(back, a, nullptr, 0, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    // Cells expected but none stored: rejected naming biome.
    BiomeIndex out[1] = {};
    EXPECT_FALSE(ReadBiomeCells(back, a, out, 1, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
}

TEST_F(HexWorldSaveCodecPalette, EmptyPaletteAcceptsOnlyZeroCells)
{
    Json::Value root;
    root["biome_palette"] = Json::Value(Json::arrayValue);
    root["biome"] = Json::Value(Json::arrayValue);
    SaveError err;
    EXPECT_TRUE(ReadBiomeCells(root, a, nullptr, 0, err));
    root["biome"].append(0);   // index 0 is outside an empty palette
    BiomeIndex out[1] = {};
    EXPECT_FALSE(ReadBiomeCells(root, a, out, 1, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
}

TEST_F(HexWorldSaveCodecPalette, PaletteSizeBoundaryAcceptsMaxRejectsOneOver)
{
    const uint32_t maxPalette = BiomeRegistry::kMaxBiomes;
    for (uint32_t size : { maxPalette, maxPalette + 1u })
    {
        Json::Value root;
        root["biome_palette"] = Json::Value(Json::arrayValue);
        for (uint32_t i = 0; i < size; ++i) root["biome_palette"].append("Biome.Ocean");
        root["biome"] = Json::Value(Json::arrayValue);
        root["biome"].append(size - 1);
        BiomeIndex out[1] = {};
        SaveError err;
        const bool ok = ReadBiomeCells(root, a, out, 1, err);
        if (size == maxPalette)
        {
            EXPECT_TRUE(ok) << err.field.AsCStr() << ": " << err.reason.AsCStr();
            EXPECT_STREQ("Biome.Ocean", a.IdText(out[0]));
        }
        else
        {
            EXPECT_FALSE(ok);
            EXPECT_STREQ("biome_palette", err.field.AsCStr());
        }
    }
}

TEST_F(HexWorldSaveCodecPalette, NegativeOrHugeCellIndexRejected)
{
    SaveError err;
    BiomeIndex out[5] = {};
    Json::Value r = WrittenUnderA();
    r["biome"][2] = -1;
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    r = WrittenUnderA();
    r["biome"][2] = Json::UInt64(4294967296ull);   // does not fit uint32
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    r = WrittenUnderA();
    r["biome"][2] = "1";
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    r = WrittenUnderA();
    r["biome"] = 5;   // not an array
    EXPECT_FALSE(ReadBiomeCells(r, a, out, 5, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    EXPECT_FALSE(ReadBiomeCells(Json::Value(Json::arrayValue), a, out, 5, err));   // root not an object
    EXPECT_STREQ("biome_palette", err.field.AsCStr());
}

TEST_F(HexWorldSaveCodecPalette, WritingAnOutOfPaletteIndexIsCaughtOnRead)
{
    // Write does not validate cells; an index past the palette (here kNoBiome) is rejected cleanly on Read.
    const BiomeIndex cells[2] = { 0, kNoBiome };
    Json::Value root;
    WriteBiomeCells(root, a, cells, 2);
    Json::Value back;
    ASSERT_TRUE(Parse(Serialize(root).c_str(), back));
    BiomeIndex out[2] = {};
    SaveError err;
    EXPECT_FALSE(ReadBiomeCells(back, a, out, 2, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
}

TEST_F(HexWorldSaveCodecPalette, MissingStoredHashWithCleanRemapLogsOneInfoAndReportsZero)
{
    Json::Value root = WrittenUnderA();
    root.removeMember("biome_palette_hash");
    EXPECT_EQ(0ull, StoredBiomeHash(root));
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, a, out, 5, err));
    Flush();
    EXPECT_EQ(1u, sink.infos);   // absent hash (0) differs from the registry hash
    EXPECT_EQ(0u, sink.warnings);
    EXPECT_EQ(0ull, StoredBiomeHash(Json::Value(Json::arrayValue)));   // non-object root
}

TEST_F(HexWorldSaveCodecPalette, HashMismatchWithUnknownIdLogsWarningButNoInfo)
{
    Json::Value root = WrittenUnderA();
    BiomeIndex out[5] = {};
    SaveError err;
    ASSERT_TRUE(ReadBiomeCells(root, d, out, 5, err));   // D lacks Scrub: not a clean remap
    Flush();
    EXPECT_EQ(0u, sink.infos);
    EXPECT_EQ(1u, sink.warnings);
}
