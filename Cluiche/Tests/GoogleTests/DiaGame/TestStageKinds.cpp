#include <gtest/gtest.h>
#include <DiaGame/StageKinds.h>
#include <DiaCore/Json/external/json/json.h>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Dia::Game;
using Dia::Core::StringCRC;

TEST(StageKinds, KindFromTag_AllKinds)
{
    StageKind k = StageKind::kTest;
    EXPECT_TRUE(KindFromTag(StringCRC("showcase"), k));
    EXPECT_EQ(k, StageKind::kShowcase);
    EXPECT_TRUE(KindFromTag(StringCRC("tool"), k));
    EXPECT_EQ(k, StageKind::kTool);
    EXPECT_TRUE(KindFromTag(StringCRC("test"), k));
    EXPECT_EQ(k, StageKind::kTest);
}

TEST(StageKinds, KindFromTag_NonKindAndCase)
{
    StageKind k = StageKind::kTool;
    EXPECT_FALSE(KindFromTag(StringCRC("physics"), k));
    EXPECT_FALSE(KindFromTag(StringCRC(""), k));
    EXPECT_FALSE(KindFromTag(StringCRC("Tool"), k));
    EXPECT_FALSE(KindFromTag(StringCRC("TEST"), k));
    EXPECT_EQ(k, StageKind::kTool);
}

TEST(StageKinds, CountAndOrder)
{
    EXPECT_EQ(kStageKindCount, 3u);
    EXPECT_STREQ(StageKindName(StageKind::kShowcase), "showcase");
    EXPECT_STREQ(StageKindName(StageKind::kTool), "tool");
    EXPECT_STREQ(StageKindName(StageKind::kTest), "test");
    EXPECT_EQ(static_cast<unsigned int>(StageKind::kShowcase), 0u);
    EXPECT_EQ(static_cast<unsigned int>(StageKind::kTool), 1u);
    EXPECT_EQ(static_cast<unsigned int>(StageKind::kTest), 2u);
}

TEST(StageKinds, MaskHelpers)
{
    EXPECT_EQ(KindBit(StageKind::kShowcase), 1);
    EXPECT_EQ(KindBit(StageKind::kTool), 2);
    EXPECT_EQ(KindBit(StageKind::kTest), 4);
    StageKindMask m = KindBit(StageKind::kShowcase) | KindBit(StageKind::kTest);
    EXPECT_TRUE(MaskHasKind(m, StageKind::kShowcase));
    EXPECT_FALSE(MaskHasKind(m, StageKind::kTool));
    EXPECT_TRUE(MaskHasKind(m, StageKind::kTest));
}

TEST(StageKinds, JsonMatchesCppTable)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path();
    fs::path found;
    for (int i = 0; i < 10; ++i)
    {
        fs::path candidate = dir / "Dia" / "DiaGame" / "stage_kinds.json";
        if (fs::exists(candidate)) { found = candidate; break; }
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    ASSERT_FALSE(found.empty()) << "stage_kinds.json not found above " << fs::current_path().string();

    std::ifstream f(found);
    std::stringstream ss;
    ss << f.rdbuf();
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(ss.str(), root));
    ASSERT_TRUE(root.isMember("kinds") && root["kinds"].isArray());
    ASSERT_EQ(root["kinds"].size(), kStageKindCount);
    for (unsigned int i = 0; i < kStageKindCount; ++i)
        EXPECT_STREQ(root["kinds"][i].asCString(), kStageKindNames[i]);
}
