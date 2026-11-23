#include <gtest/gtest.h>
#include <DraoiHexWorld/HexWorldSaveCodec.h>

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
