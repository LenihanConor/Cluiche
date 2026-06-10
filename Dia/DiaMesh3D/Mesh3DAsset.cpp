#include <DiaMesh3D/Mesh3DAsset.h>

namespace Dia { namespace Mesh3D {

Mesh3DAsset::Mesh3DAsset(Dia::Core::StringCRC assetId)
    : mAssetId(assetId)
    , mState(State::Pending)
    , mBounds()
{
}

Dia::Core::StringCRC Mesh3DAsset::GetAssetId() const
{
    return mAssetId;
}

Mesh3DAsset::State Mesh3DAsset::GetState() const
{
    return mState;
}

bool Mesh3DAsset::IsReady() const
{
    return mState == State::Ready;
}

const Dia::Core::Containers::DynamicArrayC<Vertex3D, Mesh3DAsset::kMaxVertices>& Mesh3DAsset::GetVertices() const
{
    return mVertices;
}

const Dia::Core::Containers::DynamicArrayC<uint16_t, Mesh3DAsset::kMaxIndices>& Mesh3DAsset::GetIndices() const
{
    return mIndices;
}

const Dia::Core::Containers::DynamicArrayC<Submesh, Mesh3DAsset::kMaxSubmeshes>& Mesh3DAsset::GetSubmeshes() const
{
    return mSubmeshes;
}

const Dia::Geometry3D::AABB& Mesh3DAsset::GetBounds() const
{
    return mBounds;
}

void Mesh3DAsset::Populate(const Vertex3D* vertices, uint32_t vertexCount,
                            const uint16_t* indices,  uint32_t indexCount,
                            const Submesh*  submeshes, uint32_t submeshCount,
                            const Dia::Geometry3D::AABB& bounds)
{
    mVertices.Assign(vertices, vertexCount);
    mIndices.Assign(indices, indexCount);
    mSubmeshes.Assign(submeshes, submeshCount);
    mBounds = bounds;
    mState  = State::Ready;
}

void Mesh3DAsset::MarkFailed(const char* /*reason*/)
{
    mState = State::Failed;
}

} }
