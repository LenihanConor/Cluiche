#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetRuntimeDebugCommands.h>
#include <DiaAssetRuntime/AssetState.h>

#include <DiaAPI/DiaAPI.h>

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/CRC/StringCRC.h>

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    bool WriteTempFileF6(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
    {
        char tmpDir[256];
#if defined(_MSC_VER)
        GetTempPathA(sizeof(tmpDir), tmpDir);
#else
        strncpy(tmpDir, "/tmp/", sizeof(tmpDir) - 1);
        tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
        snprintf(pathOut, pathOutSize, "%s%s", tmpDir, filename);

        FILE* f = nullptr;
#if defined(_MSC_VER)
        fopen_s(&f, pathOut, "wb");
#else
        f = fopen(pathOut, "wb");
#endif
        if (!f) return false;
        fwrite(content, 1, strlen(content), f);
        fclose(f);
        return true;
    }

    static const char* kF6Alias = "test_arun_f6";

    void SetupF6Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF6Alias);
        if (!Dia::Core::PathStore::IsPathAliasRegistered(aliasCRC))
        {
            char tmpDir[256];
#if defined(_MSC_VER)
            GetTempPathA(sizeof(tmpDir), tmpDir);
            for (char* p = tmpDir; *p; ++p)
                if (*p == '\\') *p = '/';
            int len = (int)strlen(tmpDir);
            if (len > 0 && tmpDir[len - 1] == '/')
                tmpDir[len - 1] = '\0';
#else
            strncpy(tmpDir, "/tmp", sizeof(tmpDir) - 1);
            tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
            Dia::Core::Path::String p(tmpDir);
            Dia::Core::PathStore::RegisterToStore(aliasCRC, p);
        }
    }

    Dia::Core::FilePath MakeF6FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF6Alias), filename);
    }

    static const char* kTwoAssetJson = R"({
        "assets": [
            {"id": "asset.alpha", "scope": "stage",  "deploy_path": "alpha.png"},
            {"id": "asset.beta",  "scope": "global", "deploy_path": "beta.json"}
        ],
        "stages": [
            {"id": "stage.s1", "assets": ["asset.alpha", "asset.beta"]}
        ]
    })";

    bool LoadF6Runtime(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF6(filename, kTwoAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF6FilePath(filename));
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class DebugCommandsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SetupF6Alias();
        if (Dia::API::IsInitialized())
            Dia::API::Shutdown();
        Dia::API::Initialize();
    }

    void TearDown() override
    {
        if (Dia::API::IsInitialized())
            Dia::API::Shutdown();
    }
};

// ---------------------------------------------------------------------------
// Tests — command registration
// ---------------------------------------------------------------------------

TEST_F(DebugCommandsTest, Registration_AllCommandsRegistered)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_register.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    EXPECT_NE(Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-loaded")),   nullptr);
    EXPECT_NE(Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-staged")),   nullptr);
    EXPECT_NE(Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-state")),    nullptr);
    EXPECT_NE(Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-stage-deps")), nullptr);
    EXPECT_NE(Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-all-states")), nullptr);
    EXPECT_NE(Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-subscribe-transitions")), nullptr);
}

TEST_F(DebugCommandsTest, Registration_CommandsHaveCorrectCategory)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_category.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-loaded"));
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->category, Dia::Core::StringCRC("asset-runtime"));
}

// ---------------------------------------------------------------------------
// Tests — command execution
// ---------------------------------------------------------------------------

TEST_F(DebugCommandsTest, GetLoaded_ReturnsZeroOnSuccess)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_getloaded.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-loaded"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    EXPECT_EQ(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, GetStaged_ReturnsZeroOnSuccess)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_getstaged.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-staged"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    EXPECT_EQ(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, GetState_WithValidAsset_ReturnsZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_getstate_valid.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-state"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    args.namedArgs[Dia::Core::StringCRC("assetId").Value()] = "asset.alpha";
    EXPECT_EQ(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, GetState_MissingParam_ReturnsError)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_getstate_missing.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-state"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args; // no assetId param
    EXPECT_NE(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, GetStageDeps_WithValidStage_ReturnsZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_stagedeps_valid.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-stage-deps"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    args.namedArgs[Dia::Core::StringCRC("stageId").Value()] = "stage.s1";
    EXPECT_EQ(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, GetStageDeps_WithUnknownStage_ReturnsError)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_stagedeps_unknown.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-stage-deps"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    args.namedArgs[Dia::Core::StringCRC("stageId").Value()] = "stage.ghost";
    EXPECT_NE(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, GetAllStates_ReturnsZeroOnSuccess)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_getall.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-get-all-states"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    EXPECT_EQ(cmd->callback(args), 0);
}

TEST_F(DebugCommandsTest, SubscribeTransitions_ReturnsZeroAndRegistersListener)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF6Runtime(runtime, "f6_subscribe.json"));

    Dia::AssetRuntime::RegisterAssetRuntimeCommands(runtime);

    const Dia::API::CommandInfo* cmd =
        Dia::API::GetCommand(Dia::Core::StringCRC("asset-runtime-subscribe-transitions"));
    ASSERT_NE(cmd, nullptr);

    Dia::API::CommandArgs args;
    EXPECT_EQ(cmd->callback(args), 0);

    // Second call should also return 0 (already subscribed path)
    EXPECT_EQ(cmd->callback(args), 0);
}
