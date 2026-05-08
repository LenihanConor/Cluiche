#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetState.h>
#include <DiaAssetRuntime/IAssetTypeHandler.h>

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
    bool WriteTempFileF4(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kF4Alias = "test_arun_f4";

    void SetupF4Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF4Alias);
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

    Dia::Core::FilePath MakeF4FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF4Alias), filename);
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

    static const char* kMultiTypeJson = R"({
        "assets": [
            {"id": "texture.player", "scope": "stage",  "deploy_path": "player.png"},
            {"id": "audio.bgm",     "scope": "global", "deploy_path": "bgm.ogg"}
        ],
        "stages": [
            {"id": "stage.s1", "assets": ["texture.player", "audio.bgm"]}
        ]
    })";

    bool LoadTwoAssetRuntimeF4(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF4(filename, kTwoAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF4FilePath(filename));
    }

    bool LoadMultiTypeRuntime(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF4(filename, kMultiTypeJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF4FilePath(filename));
    }

    // Handler that records which assets it was asked to load
    struct RecordingHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        static const unsigned int kMaxEvents = 32;
        Dia::Core::StringCRC loadedIds[kMaxEvents];
        unsigned int loadCount = 0;
        Dia::Core::StringCRC unloadedIds[kMaxEvents];
        unsigned int unloadCount = 0;
        bool shouldFail = false;

        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            if (loadCount < kMaxEvents)
                loadedIds[loadCount] = assetId;
            loadCount++;

            if (shouldFail)
                callback->OnLoadFailed(assetId, "forced failure");
            else
                callback->OnLoadComplete(assetId);
        }

        void Unload(const Dia::Core::StringCRC& assetId) override
        {
            if (unloadCount < kMaxEvents)
                unloadedIds[unloadCount] = assetId;
            unloadCount++;
        }
    };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class HandlerDispatchTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupF4Alias(); }
};

// ---------------------------------------------------------------------------
// Tests — handler dispatch on load
// ---------------------------------------------------------------------------

TEST_F(HandlerDispatchTest, SingleHandler_ReceivesAllAssetsOfType)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_dispatch_single.json"));

    RecordingHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(handler.loadCount, 2u);
}

TEST_F(HandlerDispatchTest, MultipleHandlers_EachReceivesOwnType)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadMultiTypeRuntime(runtime, "f4_multi_handler.json"));

    RecordingHandler textureHandler, audioHandler;
    runtime.RegisterTypeHandler("texture", &textureHandler);
    runtime.RegisterTypeHandler("audio", &audioHandler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(textureHandler.loadCount, 1u);
    EXPECT_EQ(audioHandler.loadCount, 1u);
    EXPECT_EQ(textureHandler.loadedIds[0], Dia::Core::StringCRC("texture.player"));
    EXPECT_EQ(audioHandler.loadedIds[0], Dia::Core::StringCRC("audio.bgm"));
}

// ---------------------------------------------------------------------------
// Tests — handler dispatch on unload
// ---------------------------------------------------------------------------

TEST_F(HandlerDispatchTest, Unload_DispatchesToCorrectHandler)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadMultiTypeRuntime(runtime, "f4_unload_dispatch.json"));

    RecordingHandler textureHandler, audioHandler;
    runtime.RegisterTypeHandler("texture", &textureHandler);
    runtime.RegisterTypeHandler("audio", &audioHandler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(textureHandler.unloadCount, 1u);
    EXPECT_EQ(audioHandler.unloadCount, 1u);
    EXPECT_EQ(textureHandler.unloadedIds[0], Dia::Core::StringCRC("texture.player"));
    EXPECT_EQ(audioHandler.unloadedIds[0], Dia::Core::StringCRC("audio.bgm"));
}

// ---------------------------------------------------------------------------
// Tests — no duplicate dispatch on double load
// ---------------------------------------------------------------------------

TEST_F(HandlerDispatchTest, DoubleLoad_NoRedispatch)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_no_redispatch.json"));

    RecordingHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    unsigned int afterFirst = handler.loadCount;

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(handler.loadCount, afterFirst); // no new dispatches
}

// ---------------------------------------------------------------------------
// Tests — no handler registered
// ---------------------------------------------------------------------------

TEST_F(HandlerDispatchTest, NoHandler_StageLoadDoesNotCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_no_handler.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    // Assets transition to Failed (no handler)
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
}

// ---------------------------------------------------------------------------
// Tests — handler failure path
// ---------------------------------------------------------------------------

TEST_F(HandlerDispatchTest, HandlerFailure_AssetInFailedState)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_handler_fail.json"));

    RecordingHandler handler;
    handler.shouldFail = true;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
    EXPECT_FALSE(runtime.IsLoadComplete(Dia::Core::StringCRC("stage.s1")));
}

// ---------------------------------------------------------------------------
// Tests — type prefix extraction
// ---------------------------------------------------------------------------

TEST_F(HandlerDispatchTest, TypePrefix_ExtractedFromAssetId)
{
    // Manifest has "texture.player" — prefix is "texture"
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadMultiTypeRuntime(runtime, "f4_prefix.json"));

    RecordingHandler textureHandler;
    runtime.RegisterTypeHandler("texture", &textureHandler);
    // Don't register audio handler — it should go to Failed
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(textureHandler.loadCount, 1u);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("texture.player")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("audio.bgm")),
              Dia::AssetRuntime::AssetState::Failed);
}
