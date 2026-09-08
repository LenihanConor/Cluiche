#pragma once

#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGeometry3D/Shapes/AABB.h>

#include <stdint.h>
#include <atomic>

namespace Dia { namespace Mesh3D {

class Mesh3DAsset
{
public:
    enum class State : unsigned char { Pending = 0, Ready = 1, Failed = 2 };

    static constexpr uint32_t kMaxVertices  = 65535;
    static constexpr uint32_t kMaxIndices   = 196608;
    static constexpr uint32_t kMaxSubmeshes = 32;
    static constexpr uint32_t kMaxFailReasonLength = 128;

    explicit Mesh3DAsset(Dia::Core::StringCRC assetId);

    Dia::Core::StringCRC GetAssetId()     const;
    State                GetState()       const;
    bool                 IsReady()        const;
    const char*          GetFailReason()  const;

    const Dia::Core::Containers::DynamicArrayC<Vertex3D, kMaxVertices>&  GetVertices()  const;
    const Dia::Core::Containers::DynamicArrayC<uint16_t, kMaxIndices>&   GetIndices()   const;
    const Dia::Core::Containers::DynamicArrayC<Submesh,  kMaxSubmeshes>& GetSubmeshes() const;
    const Dia::Geometry3D::AABB&                                         GetBounds()    const;

    void Populate(const Vertex3D* vertices, uint32_t vertexCount,
                  const uint16_t* indices,  uint32_t indexCount,
                  const Submesh*  submeshes, uint32_t submeshCount,
                  const Dia::Geometry3D::AABB& bounds);

    void MarkFailed(const char* reason);

private:
    Dia::Core::StringCRC    mAssetId;
    std::atomic<State>      mState;

    Dia::Core::Containers::DynamicArrayC<Vertex3D, kMaxVertices>  mVertices;
    Dia::Core::Containers::DynamicArrayC<uint16_t, kMaxIndices>   mIndices;
    Dia::Core::Containers::DynamicArrayC<Submesh,  kMaxSubmeshes> mSubmeshes;
    Dia::Geometry3D::AABB                                         mBounds;
    char                    mFailReason[kMaxFailReasonLength] = {};
};

} }
