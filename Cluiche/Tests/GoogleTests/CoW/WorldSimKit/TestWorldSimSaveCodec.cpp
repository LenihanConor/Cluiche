#include <gtest/gtest.h>

#include <string>

#include <DiaAttribute/ParameterFile.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveResult.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <direct.h>
#include <cstdio>

#include <Shared/WorldStep/WorldSimSaveCodec.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

using Draoi::HexWorld::SavedField;
using Draoi::HexWorld::SavedKind;
using Draoi::HexWorld::SavedScope;
using Draoi::HexWorld::SaveError;

namespace {

// One dia.value_schema document with one float field entry and an inclusive [min, max].
std::string SchemaText(const char* forName, const char* key, double mn, double mx)
{
    Json::Value entry(Json::objectValue);
    entry["key"] = key;
    entry["kind"] = "field";
    entry["label"] = "Label";
    entry["description"] = "Desc";
    entry["type"] = "float";
    entry["min"] = mn;
    entry["max"] = mx;
    Json::Value doc(Json::objectValue);
    doc["format"] = "dia.value_schema";
    doc["version"] = 1;
    doc["for"] = forName;
    doc["entries"] = Json::Value(Json::arrayValue);
    doc["entries"].append(entry);
    Json::FastWriter w;
    return w.write(doc);
}

bool AddDoc(Dia::ValueSchema::ValueSchemaRegistry& reg, const std::string& text)
{
    Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text.c_str(), "fixture.json");
    if (!r.mOk) { return false; }
    for (unsigned i = 0; i < r.mEntries.Size(); ++i)
    {
        if (reg.Register(r.mEntries[i]) != Dia::ValueSchema::RegisterResult::Ok) { return false; }
    }
    return true;
}

// A ready record of 2x2 cells with one per-cell array.
Json::Value Record(const char* arrayName, const float (&values)[4], uint64_t paramsHash = 7)
{
    Json::Value root(Json::objectValue);
    Draoi::HexWorld::SaveHeader h;
    h.width = 2; h.height = 2; h.cellCount = 4; h.stepDay = 3; h.paramsHash = paramsHash; h.basinCount = 0; h.worldHash = 9;
    Draoi::HexWorld::WriteHeader(root, h);
    Draoi::HexWorld::WriteArray(root, arrayName, values, 4);
    return root;
}

bool ValidateWith(const SavedField& f, const Json::Value& record, SaveError& err)
{
    Draoi::HexWorld::Caps caps;
    return Draoi::HexWorld::Validate(record, &f, 1, caps, err);
}

}  // namespace

// ---- Task 15b: ranges from sidecars ----------------------------------------------------------------

TEST(WorldSimSaveCodecTest, LayerSidecarMaxDrivesValidateWithNoCodeChange)
{
    const FieldDecl decl = { "cloud", SavedKind::F32, SavedScope::PerCell };
    const float ok[4] = { 0.f, 0.5f, 1.f, 1.f };
    const float high[4] = { 0.f, 0.5f, 1.f, 2.f };

    {
        Dia::ValueSchema::ValueSchemaRegistry reg;
        ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 1.0)));
        reg.Publish();
        SavedField f;
        SaveError err;
        ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
        EXPECT_EQ(1.0, f.max);
        EXPECT_TRUE(ValidateWith(f, Record("cloud", ok), err));
        EXPECT_FALSE(ValidateWith(f, Record("cloud", high), err));
        EXPECT_STREQ("cloud", err.field.AsCStr());
    }
    {
        // Same code, sidecar max changed to 3: the value 2 is now accepted.
        Dia::ValueSchema::ValueSchemaRegistry reg;
        ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 3.0)));
        reg.Publish();
        SavedField f;
        SaveError err;
        ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
        EXPECT_TRUE(ValidateWith(f, Record("cloud", high), err));
    }
}

TEST(WorldSimSaveCodecTest, ResolvesThroughTheServiceToo)
{
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.hydro.layers", "flow", -2.0, 5.0)));
    reg.Publish();
    Dia::ValueSchema::ValueSchemaService svc(reg);
    const FieldDecl decl = { "flow", SavedKind::F32, SavedScope::PerCell };
    SavedField f;
    SaveError err;
    ASSERT_TRUE(ResolveSavedFields(svc, "hydro", &decl, 1, &f, err));
    EXPECT_EQ(-2.0, f.min);
    EXPECT_EQ(5.0, f.max);
}

TEST(WorldSimSaveCodecTest, StateFieldWithoutLayerEntryReadsTheStateSchema)
{
    const FieldDecl decl = { "snow_depth", SavedKind::F32, SavedScope::PerCell };
    const float v[4] = { 0.f, 1.f, 4.f, 4.f };
    const float tooBig[4] = { 0.f, 1.f, 4.f, 5.f };
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather_state", "snow_depth", 0.0, 4.0)));
    reg.Publish();
    SavedField f;
    SaveError err;
    ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
    EXPECT_EQ(4.0, f.max);
    EXPECT_TRUE(ValidateWith(f, Record("snow_depth", v), err));
    EXPECT_FALSE(ValidateWith(f, Record("snow_depth", tooBig), err));
}

TEST(WorldSimSaveCodecTest, LayerEntryWinsOverStateEntry)
{
    const FieldDecl decl = { "cloud", SavedKind::F32, SavedScope::PerCell };
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 1.0)));
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather_state", "cloud", 0.0, 9.0)));
    reg.Publish();
    SavedField f;
    SaveError err;
    ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
    EXPECT_EQ(1.0, f.max);
}

TEST(WorldSimSaveCodecTest, FieldWithNoEntryOrNoRangeIsRefusedNamingTheField)
{
    const FieldDecl missing = { "ghost", SavedKind::F32, SavedScope::PerCell };
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 1.0)));
    reg.Publish();
    SavedField f;
    SaveError err;
    EXPECT_FALSE(ResolveSavedFields(reg, "weather", &missing, 1, &f, err));
    EXPECT_STREQ("ghost", err.field.AsCStr());

    // Unknown system token: no key can be built.
    const FieldDecl cloud = { "cloud", SavedKind::F32, SavedScope::PerCell };
    EXPECT_FALSE(ResolveSavedFields(reg, "nope", &cloud, 1, &f, err));
    EXPECT_STREQ("cloud", err.field.AsCStr());
}

// ---- Task 17: params_hash and versioning -----------------------------------------------------------

namespace {

const char* kParamsSidecar =
    "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"test.params\",\"entries\":["
    "{\"key\":\"a\",\"kind\":\"parameter\",\"label\":\"A\",\"description\":\"d\",\"type\":\"float\",\"min\":0,\"max\":10,\"default\":1}"
    "]}";

}  // namespace

TEST(WorldSimSaveCodecTest, ParamsHashFromParameterFileMatchesAndDetectsAChange)
{
    Dia::Attribute::ParameterFile file;
    Dia::Attribute::ParamError perr;
    ASSERT_TRUE(file.Load(kParamsSidecar, "params.json", nullptr, perr));
    const uint64_t hash = file.Hash();

    const float v[4] = { 0.f, 0.f, 0.f, 0.f };
    const Json::Value record = Record("cloud", v, hash);
    EXPECT_TRUE(ParamsHashMatches(record, hash));

    Json::Value value(2.5);
    ASSERT_TRUE(file.Set(Dia::Core::StringCRC("a"), value, perr));
    EXPECT_NE(hash, file.Hash());
    EXPECT_FALSE(ParamsHashMatches(record, file.Hash()));
}

TEST(WorldSimSaveCodecTest, ParamsHashMatchesRefusesRecordsWithoutAHash)
{
    Json::Value notReady(Json::objectValue);
    Draoi::HexWorld::WriteNotReady(notReady);
    EXPECT_FALSE(ParamsHashMatches(notReady, 0));
    EXPECT_FALSE(ParamsHashMatches(Json::Value(Json::nullValue), 0));
}

TEST(WorldSimSaveCodecTest, SaveIdIsCowDotSystem)
{
    EXPECT_STREQ("cow.weather", World::SaveId("weather").c_str());
    EXPECT_STREQ("cow.hydro", World::SaveId("hydro").c_str());
}

namespace {

// A tiny participant: v2 expects a `layout` int that v1 records do not have; the migration adds it.
struct FakeRecordSaveable : Dia::SaveGame::ISaveable
{
    uint32_t version = 1;
    int32_t layout = 0;
    bool sawVersionKey = false;
    void Serialize(Dia::SaveGame::SaveContext& ctx) const override
    {
        ctx.Write(Dia::Core::StringCRC("ready"), true);
        ctx.Write(Dia::Core::StringCRC("step_day"), static_cast<int32_t>(5));
        if (version >= 2) { ctx.Write(Dia::Core::StringCRC("layout"), static_cast<int32_t>(2)); }
    }
    void Deserialize(Dia::SaveGame::LoadContext& ctx) override
    {
        sawVersionKey = ctx.CurrentNode().isMember("version");
        ctx.Read(Dia::Core::StringCRC("layout"), layout);
    }
    uint32_t GetVersion() const override { return version; }
};

}  // namespace

TEST(WorldSimSaveCodecTest, V1RecordLoadsAsV2ThroughARegisteredMigration)
{
    _mkdir("temp");
    _mkdir("temp/worldsim_codec_tests");
    const Dia::Core::StringCRC id(World::SaveId("weather").c_str());
    Dia::SaveGame::SaveConfig cfg{ "temp/worldsim_codec_tests/", "slot_{id}.sav", 0, Dia::SaveGame::SaveFormat::Json };

    FakeRecordSaveable v1;
    v1.version = 1;
    Dia::SaveGame::SaveRegistry saveReg;
    saveReg.Register(id, &v1);
    Dia::SaveGame::SaveManager saveMgr;
    saveMgr.Init(cfg, saveReg);
    ASSERT_TRUE(saveMgr.Save(Dia::Core::StringCRC("wsc_v1")).Ok());

    FakeRecordSaveable v2;
    v2.version = 2;
    Dia::SaveGame::SaveRegistry loadReg;
    loadReg.Register(id, &v2);
    bool migrated = false;
    loadReg.RegisterMigration(id, 1, [&migrated](Dia::SaveGame::LoadContext& ctx) {
        migrated = true;
        ctx.Write(Dia::Core::StringCRC("layout"), static_cast<int32_t>(2));
    });
    Dia::SaveGame::SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);
    const Dia::SaveGame::LoadResult lr = loadMgr.Load(Dia::Core::StringCRC("wsc_v1"));
    ASSERT_TRUE(lr.Ok());
    EXPECT_TRUE(migrated);
    EXPECT_EQ(2, v2.layout);
    EXPECT_FALSE(v2.sawVersionKey) << "no in-record version field";
    EXPECT_EQ(2u, v2.GetVersion());
    std::remove("temp/worldsim_codec_tests/slot_wsc_v1.sav");
}

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
