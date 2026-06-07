#pragma once
#include "DiaGraphics3D/Camera3D.h"
#include "DiaGraphics3D/Light.h"
#include "DiaGraphics3D/Mesh3DDrawCommand.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Graphics3D {

class Mesh3DFrameData
{
public:
    static constexpr uint32_t kMaxMeshDraws = 4096;
    static constexpr uint32_t kMaxLights    = 32;

    Mesh3DFrameData();

    void RequestDrawMesh(const Mesh3DDrawCommand& cmd);
    void SetCamera(const Camera3D& camera);
    void AddDirectionalLight(const DirectionalLight& light);
    void AddPointLight(const PointLight& light);

    void Clear();
    void Copy(const Mesh3DFrameData& rhs);

    const Camera3D& GetCamera() const;
    const Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand, kMaxMeshDraws>& GetMeshDraws()         const;
    const Dia::Core::Containers::DynamicArrayC<DirectionalLight,  kMaxLights>&    GetDirectionalLights() const;
    const Dia::Core::Containers::DynamicArrayC<PointLight,        kMaxLights>&    GetPointLights()       const;

    uint32_t DroppedMeshCount()  const;
    uint32_t DroppedLightCount() const;

private:
    Camera3D                                                                       mCamera;
    Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand, kMaxMeshDraws>         mMeshDraws;
    Dia::Core::Containers::DynamicArrayC<DirectionalLight,  kMaxLights>            mDirectionalLights;
    Dia::Core::Containers::DynamicArrayC<PointLight,        kMaxLights>            mPointLights;
    uint32_t                                                                       mDroppedMeshes;
    uint32_t                                                                       mDroppedLights;
    bool                                                                           mMeshOverCapacityLogged              = false;
    bool                                                                           mDirectionalLightOverCapacityLogged  = false;
    bool                                                                           mPointLightOverCapacityLogged        = false;
};

} }
