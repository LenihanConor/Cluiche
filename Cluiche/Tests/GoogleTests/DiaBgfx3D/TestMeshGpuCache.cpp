// TestMeshGpuCache.cpp - Google Test unit tests for MeshGpuCache
//
// Only the not-Ready path is tested here. The Ready path (GPU upload via bgfx)
// requires a live bgfx context and cannot run headlessly.
// MeshGpuCache hides bgfx behind pimpl — no bgfx headers are needed in this file.

#include <gtest/gtest.h>
#include <DiaBgfx3D/Resources/MeshGpuCache.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaCore/CRC/StringCRC.h>

using Dia::Bgfx3D::MeshGpuCache;
using Dia::Mesh3D::Mesh3DAsset;
using Dia::Core::StringCRC;

// ==============================================================================
// GetOrUpload — not-Ready asset paths (AC-5)
// ==============================================================================

TEST(DiaBgfx3D_MeshGpuCacheTest, GetOrUploadReturnNullForPendingAsset)
{
    // Default-constructed Mesh3DAsset state is Pending.
    Mesh3DAsset asset(StringCRC("mesh_pending"));

    ASSERT_FALSE(asset.IsReady());

    MeshGpuCache cache;
    const Dia::Bgfx3D::GpuMesh* result = cache.GetOrUpload(asset);

    EXPECT_EQ(result, nullptr);
}

TEST(DiaBgfx3D_MeshGpuCacheTest, GetOrUploadReturnNullForFailedAsset)
{
    Mesh3DAsset asset(StringCRC("mesh_failed"));
    asset.MarkFailed("test failure");

    ASSERT_FALSE(asset.IsReady());

    MeshGpuCache cache;
    const Dia::Bgfx3D::GpuMesh* result = cache.GetOrUpload(asset);

    EXPECT_EQ(result, nullptr);
}

// ==============================================================================
// DestroyAll — empty cache (AC-6)
// ==============================================================================

TEST(DiaBgfx3D_MeshGpuCacheTest, DestroyAllOnEmptyCacheDoesNotCrash)
{
    MeshGpuCache cache;

    // DestroyAll on an empty cache must not crash.
    cache.DestroyAll();
}
