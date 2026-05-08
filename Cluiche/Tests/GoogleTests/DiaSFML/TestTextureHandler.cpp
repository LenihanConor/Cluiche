#include <gtest/gtest.h>

#include <DiaSFML/TextureHandler.h>
#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>

TEST(TextureHandlerTest, GetTextureId_UnknownAsset_ReturnsZero)
{
    Dia::SFML::TextureHandler handler;

    Dia::Core::StringCRC assetId("texture.unknown");
    EXPECT_EQ(handler.GetTextureId(assetId), 0u);
}

TEST(TextureHandlerTest, GetTexture_InvalidId_ReturnsNull)
{
    Dia::SFML::TextureHandler handler;
    EXPECT_EQ(handler.GetTexture(999), nullptr);
}

TEST(TextureHandlerTest, Unload_UnknownAsset_DoesNotCrash)
{
    Dia::SFML::TextureHandler handler;

    Dia::Core::StringCRC assetId("texture.unknown");
    handler.Unload(assetId);
}

TEST(TextureHandlerTest, GetLoadedCount_Empty_ReturnsZero)
{
    Dia::SFML::TextureHandler handler;
    EXPECT_EQ(handler.GetLoadedCount(), 0u);
}
