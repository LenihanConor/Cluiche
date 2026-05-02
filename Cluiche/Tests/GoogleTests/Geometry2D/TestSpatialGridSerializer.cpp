#include <gtest/gtest.h>

#include <DiaGeometry2D/Spatial/JsonSpatialGridSerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <cstdio>

using namespace Dia::Geometry2D;

static const char* kValidSpatialGrid = R"({
    "world_bounds": {
        "bottom_left": [-100, -100],
        "top_right": [100, 100]
    },
    "cell_size": 5.0
})";

static const char* kValidHexGrid = R"({
    "world_bounds": {
        "bottom_left": [0, 0],
        "top_right": [200, 200]
    },
    "hex_radius": 3.0
})";

static const char* kMissingWorldBounds = R"({
    "cell_size": 5.0
})";

static const char* kMissingCellSize = R"({
    "world_bounds": {
        "bottom_left": [0, 0],
        "top_right": [10, 10]
    }
})";

static const char* kZeroCellSize = R"({
    "world_bounds": {
        "bottom_left": [0, 0],
        "top_right": [10, 10]
    },
    "cell_size": 0.0
})";

static const char* kMalformed = R"({ not valid )";

static const char* kTmpSpatialPath = "C:\\Temp\\dia_test_spatialgrid.json";
static const char* kTmpHexPath     = "C:\\Temp\\dia_test_hexgrid.json";

// ---------------------------------------------------------------------------
// SpatialGrid
// ---------------------------------------------------------------------------

TEST(SpatialGridSerializer, Load_SpatialGrid_Valid)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    ASSERT_TRUE(s.LoadSpatialGrid(kValidSpatialGrid, cfg));

    EXPECT_FLOAT_EQ(cfg.cellSize, 5.0f);
    EXPECT_FLOAT_EQ(cfg.worldBounds.GetBottomLeft().X(), -100.0f);
    EXPECT_FLOAT_EQ(cfg.worldBounds.GetBottomLeft().Y(), -100.0f);
    EXPECT_FLOAT_EQ(cfg.worldBounds.GetTopRight().X(),   100.0f);
    EXPECT_FLOAT_EQ(cfg.worldBounds.GetTopRight().Y(),   100.0f);
}

TEST(SpatialGridSerializer, Load_SpatialGrid_MissingWorldBounds_Fails)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    auto result = s.LoadSpatialGrid(kMissingWorldBounds, cfg);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(SpatialGridSerializer, Load_SpatialGrid_MissingCellSize_Fails)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    EXPECT_FALSE(s.LoadSpatialGrid(kMissingCellSize, cfg));
}

TEST(SpatialGridSerializer, Load_SpatialGrid_ZeroCellSize_Fails)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    EXPECT_FALSE(s.LoadSpatialGrid(kZeroCellSize, cfg));
}

TEST(SpatialGridSerializer, Load_SpatialGrid_Malformed_Fails)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    auto result = s.LoadSpatialGrid(kMalformed, cfg);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
    EXPECT_GT(strlen(result.error), 0u);
}

TEST(SpatialGridSerializer, RoundTrip_SpatialGrid)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    ASSERT_TRUE(s.LoadSpatialGrid(kValidSpatialGrid, cfg));

    char buffer[2048];
    ASSERT_TRUE(s.SaveSpatialGrid(cfg, buffer, sizeof(buffer)));

    SpatialGridConfig cfg2;
    ASSERT_TRUE(s.LoadSpatialGrid(buffer, cfg2));

    EXPECT_FLOAT_EQ(cfg2.cellSize, cfg.cellSize);
    EXPECT_FLOAT_EQ(cfg2.worldBounds.GetBottomLeft().X(), cfg.worldBounds.GetBottomLeft().X());
    EXPECT_FLOAT_EQ(cfg2.worldBounds.GetTopRight().Y(),   cfg.worldBounds.GetTopRight().Y());
}

TEST(SpatialGridSerializer, SaveToFileAndLoadFromFile_SpatialGrid)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    ASSERT_TRUE(s.LoadSpatialGrid(kValidSpatialGrid, cfg));

    ASSERT_TRUE(s.SaveSpatialGridToFile(kTmpSpatialPath, cfg));

    SpatialGridConfig loaded;
    ASSERT_TRUE(s.LoadSpatialGridFromFile(kTmpSpatialPath, loaded));

    EXPECT_FLOAT_EQ(loaded.cellSize, cfg.cellSize);
    EXPECT_FLOAT_EQ(loaded.worldBounds.GetBottomLeft().X(), cfg.worldBounds.GetBottomLeft().X());

    remove(kTmpSpatialPath);
}

TEST(SpatialGridSerializer, LoadSpatialGridFromFile_Missing_Fails)
{
    JsonSpatialGridSerializer s;
    SpatialGridConfig cfg;
    auto result = s.LoadSpatialGridFromFile("C:\\Temp\\nonexistent_sg.json", cfg);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

// ---------------------------------------------------------------------------
// HexGrid
// ---------------------------------------------------------------------------

TEST(SpatialGridSerializer, Load_HexGrid_Valid)
{
    JsonSpatialGridSerializer s;
    HexGridConfig cfg;
    ASSERT_TRUE(s.LoadHexGrid(kValidHexGrid, cfg));

    EXPECT_FLOAT_EQ(cfg.hexRadius, 3.0f);
    EXPECT_FLOAT_EQ(cfg.worldBounds.GetBottomLeft().X(), 0.0f);
    EXPECT_FLOAT_EQ(cfg.worldBounds.GetTopRight().X(), 200.0f);
}

TEST(SpatialGridSerializer, Load_HexGrid_MissingHexRadius_Fails)
{
    JsonSpatialGridSerializer s;
    HexGridConfig cfg;

    static const char* kNoRadius = R"({
        "world_bounds": { "bottom_left": [0,0], "top_right": [10,10] }
    })";

    EXPECT_FALSE(s.LoadHexGrid(kNoRadius, cfg));
}

TEST(SpatialGridSerializer, Load_HexGrid_ZeroRadius_Fails)
{
    JsonSpatialGridSerializer s;
    HexGridConfig cfg;

    static const char* kZero = R"({
        "world_bounds": { "bottom_left": [0,0], "top_right": [10,10] },
        "hex_radius": 0.0
    })";

    EXPECT_FALSE(s.LoadHexGrid(kZero, cfg));
}

TEST(SpatialGridSerializer, RoundTrip_HexGrid)
{
    JsonSpatialGridSerializer s;
    HexGridConfig cfg;
    ASSERT_TRUE(s.LoadHexGrid(kValidHexGrid, cfg));

    char buffer[2048];
    ASSERT_TRUE(s.SaveHexGrid(cfg, buffer, sizeof(buffer)));

    HexGridConfig cfg2;
    ASSERT_TRUE(s.LoadHexGrid(buffer, cfg2));

    EXPECT_FLOAT_EQ(cfg2.hexRadius, cfg.hexRadius);
    EXPECT_FLOAT_EQ(cfg2.worldBounds.GetBottomLeft().X(), cfg.worldBounds.GetBottomLeft().X());
    EXPECT_FLOAT_EQ(cfg2.worldBounds.GetTopRight().Y(),   cfg.worldBounds.GetTopRight().Y());
}

TEST(SpatialGridSerializer, SaveToFileAndLoadFromFile_HexGrid)
{
    JsonSpatialGridSerializer s;
    HexGridConfig cfg;
    ASSERT_TRUE(s.LoadHexGrid(kValidHexGrid, cfg));

    ASSERT_TRUE(s.SaveHexGridToFile(kTmpHexPath, cfg));

    HexGridConfig loaded;
    ASSERT_TRUE(s.LoadHexGridFromFile(kTmpHexPath, loaded));

    EXPECT_FLOAT_EQ(loaded.hexRadius, cfg.hexRadius);

    remove(kTmpHexPath);
}

TEST(SpatialGridSerializer, LoadHexGridFromFile_Missing_Fails)
{
    JsonSpatialGridSerializer s;
    HexGridConfig cfg;
    auto result = s.LoadHexGridFromFile("C:\\Temp\\nonexistent_hex.json", cfg);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(SpatialGridSerializer, GetVersion_NotEmpty)
{
    JsonSpatialGridSerializer s;
    const char* v = s.GetVersion();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(strlen(v), 0u);
}
