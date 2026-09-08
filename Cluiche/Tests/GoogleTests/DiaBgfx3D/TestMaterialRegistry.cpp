// TestMaterialRegistry.cpp - Google Test unit tests for MaterialRegistry
//
// Tests the material registry lookup logic. No bgfx context required —
// all tests exercise the register/resolve/default paths only.
// program pointers are nullptr throughout (no bgfx context in headless runner).

#include <gtest/gtest.h>
#include <DiaBgfx3D/Resources/MaterialRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

using Dia::Bgfx3D::MaterialDescriptor;
using Dia::Bgfx3D::MaterialRegistry;
using Dia::Core::StringCRC;

// ==============================================================================
// Resolve tests
// ==============================================================================

TEST(DiaBgfx3D_MaterialRegistryTest, ResolveReturnsNullForUnknownId)
{
    MaterialRegistry registry;

    const MaterialDescriptor* result = registry.Resolve(StringCRC("mat_unknown"));

    EXPECT_EQ(result, nullptr);
}

TEST(DiaBgfx3D_MaterialRegistryTest, RegisterAndResolveById)
{
    MaterialRegistry registry;

    MaterialDescriptor desc;
    desc.id             = StringCRC("mat_test");
    desc.program        = nullptr;
    desc.baseColourRGBA = 0xDEADBEEFu;

    registry.Register(desc);

    const MaterialDescriptor* result = registry.Resolve(StringCRC("mat_test"));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->id,             StringCRC("mat_test"));
    EXPECT_EQ(result->baseColourRGBA, 0xDEADBEEFu);
    EXPECT_EQ(result->program,        nullptr);
}

TEST(DiaBgfx3D_MaterialRegistryTest, RegisterOverwritesDuplicate)
{
    MaterialRegistry registry;

    MaterialDescriptor first;
    first.id             = StringCRC("mat_overwrite");
    first.program        = nullptr;
    first.baseColourRGBA = 0x11111111u;

    MaterialDescriptor second;
    second.id             = StringCRC("mat_overwrite");
    second.program        = nullptr;
    second.baseColourRGBA = 0x22222222u;

    registry.Register(first);
    registry.Register(second);

    const MaterialDescriptor* result = registry.Resolve(StringCRC("mat_overwrite"));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->baseColourRGBA, 0x22222222u);
}

TEST(DiaBgfx3D_MaterialRegistryTest, GetDefaultReturnsValidDescriptor)
{
    MaterialRegistry registry;

    const MaterialDescriptor& def = registry.GetDefault();

    EXPECT_EQ(def.baseColourRGBA, 0xFFFFFFFFu);
    EXPECT_EQ(def.id,             StringCRC("default"));
}

TEST(DiaBgfx3D_MaterialRegistryTest, ResolveReturnsNullAfterDifferentIdRegistered)
{
    MaterialRegistry registry;

    MaterialDescriptor desc;
    desc.id             = StringCRC("foo");
    desc.program        = nullptr;
    desc.baseColourRGBA = 0xAABBCCDDu;

    registry.Register(desc);

    const MaterialDescriptor* result = registry.Resolve(StringCRC("bar"));

    EXPECT_EQ(result, nullptr);
}

TEST(DiaBgfx3D_MaterialRegistryTest, RegisterUpToCapacity)
{
    MaterialRegistry registry;

    // Register exactly kMaxMaterials materials.
    for (unsigned int i = 0; i < MaterialRegistry::kMaxMaterials; ++i)
    {
        // Build a unique CRC-string per slot.
        // We use a simple buffer — StringCRC hashes the literal at construction.
        char nameBuf[32];
        // Format: "mat_XXXXXXXX" (hex index, zero-padded to 8 chars)
        const char* const hexDigits = "0123456789abcdef";
        nameBuf[0]  = 'm'; nameBuf[1] = 'a'; nameBuf[2] = 't'; nameBuf[3] = '_';
        nameBuf[4]  = hexDigits[(i >> 28) & 0xF];
        nameBuf[5]  = hexDigits[(i >> 24) & 0xF];
        nameBuf[6]  = hexDigits[(i >> 20) & 0xF];
        nameBuf[7]  = hexDigits[(i >> 16) & 0xF];
        nameBuf[8]  = hexDigits[(i >> 12) & 0xF];
        nameBuf[9]  = hexDigits[(i >>  8) & 0xF];
        nameBuf[10] = hexDigits[(i >>  4) & 0xF];
        nameBuf[11] = hexDigits[(i >>  0) & 0xF];
        nameBuf[12] = '\0';

        MaterialDescriptor desc;
        desc.id             = StringCRC(nameBuf);
        desc.program        = nullptr;
        desc.baseColourRGBA = static_cast<uint32_t>(i);

        registry.Register(desc);
    }

    // All registered entries must be resolvable with the correct colour.
    for (unsigned int i = 0; i < MaterialRegistry::kMaxMaterials; ++i)
    {
        char nameBuf[32];
        const char* const hexDigits = "0123456789abcdef";
        nameBuf[0]  = 'm'; nameBuf[1] = 'a'; nameBuf[2] = 't'; nameBuf[3] = '_';
        nameBuf[4]  = hexDigits[(i >> 28) & 0xF];
        nameBuf[5]  = hexDigits[(i >> 24) & 0xF];
        nameBuf[6]  = hexDigits[(i >> 20) & 0xF];
        nameBuf[7]  = hexDigits[(i >> 16) & 0xF];
        nameBuf[8]  = hexDigits[(i >> 12) & 0xF];
        nameBuf[9]  = hexDigits[(i >>  8) & 0xF];
        nameBuf[10] = hexDigits[(i >>  4) & 0xF];
        nameBuf[11] = hexDigits[(i >>  0) & 0xF];
        nameBuf[12] = '\0';

        const MaterialDescriptor* result = registry.Resolve(StringCRC(nameBuf));
        ASSERT_NE(result, nullptr) << "Expected non-null for slot " << i;
        EXPECT_EQ(result->baseColourRGBA, static_cast<uint32_t>(i)) << "Colour mismatch at slot " << i;
    }

    // Registering a 257th distinct material must not crash (it will be silently dropped).
    MaterialDescriptor overflow;
    overflow.id             = StringCRC("mat_overflow");
    overflow.program        = nullptr;
    overflow.baseColourRGBA = 0xFFFFFFFFu;

    registry.Register(overflow);  // must not crash or corrupt state
}
