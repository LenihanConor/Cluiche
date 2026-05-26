#include <gtest/gtest.h>

#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>

TEST(TextureHandlerTest, LookupTexture_UnknownAsset_ReturnsNull)
{
    Dia::AssetRuntime::TextureHandler handler;

    Dia::Core::StringCRC assetId("texture.unknown");
    EXPECT_EQ(handler.LookupTexture(assetId), nullptr);
}

TEST(TextureHandlerTest, Unload_UnknownAsset_DoesNotCrash)
{
    Dia::AssetRuntime::TextureHandler handler;

    Dia::Core::StringCRC assetId("texture.unknown");
    handler.Unload(assetId);
}

TEST(TextureHandlerTest, GetLoadedCount_Empty_ReturnsZero)
{
    Dia::AssetRuntime::TextureHandler handler;
    EXPECT_EQ(handler.GetLoadedCount(), 0u);
}
