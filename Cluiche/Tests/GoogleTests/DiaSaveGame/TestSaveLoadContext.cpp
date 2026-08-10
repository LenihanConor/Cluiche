#include <gtest/gtest.h>

#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Primitive round-trips
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Context, RoundTripInt)
{
    SaveContext save;
    save.Write(StringCRC("hp"), int32_t(42));

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    int32_t hp = 0;
    EXPECT_TRUE(load.Read(StringCRC("hp"), hp));
    EXPECT_EQ(42, hp);
}

TEST(DiaSaveGame_Context, RoundTripFloat)
{
    SaveContext save;
    save.Write(StringCRC("speed"), 3.14f);

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    float speed = 0.0f;
    EXPECT_TRUE(load.Read(StringCRC("speed"), speed));
    EXPECT_NEAR(3.14f, speed, 0.0001f);
}

TEST(DiaSaveGame_Context, RoundTripBool)
{
    SaveContext save;
    save.Write(StringCRC("alive"), true);
    save.Write(StringCRC("dead"), false);

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    bool alive = false;
    bool dead  = true;
    EXPECT_TRUE(load.Read(StringCRC("alive"), alive));
    EXPECT_TRUE(load.Read(StringCRC("dead"),  dead));
    EXPECT_TRUE(alive);
    EXPECT_FALSE(dead);
}

TEST(DiaSaveGame_Context, RoundTripString)
{
    SaveContext save;
    save.Write(StringCRC("name"), "Dia");

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    char name[32] = {};
    EXPECT_TRUE(load.Read(StringCRC("name"), name, sizeof(name)));
    EXPECT_STREQ("Dia", name);
}

// ---------------------------------------------------------------------------
// Nested object round-trip
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Context, NestedObject)
{
    SaveContext save;
    save.Write(StringCRC("level"), int32_t(1));
    save.BeginObject(StringCRC("stats"));
        save.Write(StringCRC("str"), int32_t(10));
        save.Write(StringCRC("dex"), int32_t(8));
    save.EndObject();

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    int32_t level = 0;
    EXPECT_TRUE(load.Read(StringCRC("level"), level));
    EXPECT_EQ(1, level);

    ASSERT_TRUE(load.BeginObject(StringCRC("stats")));
    int32_t str = 0, dex = 0;
    EXPECT_TRUE(load.Read(StringCRC("str"), str));
    EXPECT_TRUE(load.Read(StringCRC("dex"), dex));
    EXPECT_EQ(10, str);
    EXPECT_EQ(8,  dex);
    load.EndObject();
}

// ---------------------------------------------------------------------------
// Array of ints round-trip
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Context, ArrayOfInts)
{
    SaveContext save;
    save.BeginArray(StringCRC("items"));
    Json::Value& arr = save.Root()["items"];
    arr.append(11);
    arr.append(22);
    arr.append(33);
    save.EndArray();

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    uint32_t count = 0;
    ASSERT_TRUE(load.BeginArray(StringCRC("items"), count));
    EXPECT_EQ(3u, count);

    int32_t vals[3] = {};
    for (uint32_t i = 0; i < count; ++i)
    {
        load.SetArrayIndex(i);
        // Array elements are plain values — read via index key "0","1","2"
        // (jsoncpp array elements accessed by index via the node itself)
        vals[i] = root["items"][i].asInt();
    }
    load.EndArray();

    EXPECT_EQ(11, vals[0]);
    EXPECT_EQ(22, vals[1]);
    EXPECT_EQ(33, vals[2]);
}

// ---------------------------------------------------------------------------
// Missing key returns false
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Context, MissingKeyReturnsFalse)
{
    Json::Value root(Json::objectValue);
    LoadContext load(root);

    int32_t i = 0;
    float   f = 0.0f;
    bool    b = false;
    char    s[8] = {};

    EXPECT_FALSE(load.Read(StringCRC("missing_int"),   i));
    EXPECT_FALSE(load.Read(StringCRC("missing_float"), f));
    EXPECT_FALSE(load.Read(StringCRC("missing_bool"),  b));
    EXPECT_FALSE(load.Read(StringCRC("missing_str"),   s, sizeof(s)));
    EXPECT_FALSE(load.BeginObject(StringCRC("missing_obj")));
}

// ---------------------------------------------------------------------------
// BeginObject on non-object key returns false
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Context, BeginObjectOnWrongType)
{
    SaveContext save;
    save.Write(StringCRC("x"), int32_t(1));

    char buf[1024];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext load(root);
    EXPECT_FALSE(load.BeginObject(StringCRC("x")));
}
