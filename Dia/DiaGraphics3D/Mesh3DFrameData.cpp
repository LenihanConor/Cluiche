#include "DiaGraphics3D/Mesh3DFrameData.h"
#include <DiaObservation/Log/DiaLog.h>

namespace Dia { namespace Graphics3D {

////////////////////////////////////////////////////////////
Mesh3DFrameData::Mesh3DFrameData()
    : mDroppedMeshes(0)
    , mDroppedLights(0)
{
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::RequestDrawMesh(const Mesh3DDrawCommand& cmd)
{
    if (mMeshDraws.IsFull())
    {
        if (!mMeshOverCapacityLogged)
        {
            DIA_LOG_WARNING("graphics3d", "Mesh3DFrameData: mesh draw budget exceeded (%u). Draw calls will be dropped.", kMaxMeshDraws);
            mMeshOverCapacityLogged = true;
        }
        ++mDroppedMeshes;
        return;
    }
    mMeshDraws.Add(cmd);
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::SetCamera(const Camera3D& camera)
{
    mCamera = camera;
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::SetAmbientLight(const AmbientLight& light)
{
    mAmbientLight = light;
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::AddDirectionalLight(const DirectionalLight& light)
{
    if (mDirectionalLights.IsFull())
    {
        if (!mDirectionalLightOverCapacityLogged)
        {
            DIA_LOG_WARNING("graphics3d", "Mesh3DFrameData: directional light budget exceeded (%u). Light will be dropped.", kMaxLights);
            mDirectionalLightOverCapacityLogged = true;
        }
        ++mDroppedLights;
        return;
    }
    mDirectionalLights.Add(light);
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::AddPointLight(const PointLight& light)
{
    if (mPointLights.IsFull())
    {
        if (!mPointLightOverCapacityLogged)
        {
            DIA_LOG_WARNING("graphics3d", "Mesh3DFrameData: point light budget exceeded (%u). Light will be dropped.", kMaxLights);
            mPointLightOverCapacityLogged = true;
        }
        ++mDroppedLights;
        return;
    }
    mPointLights.Add(light);
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::Clear()
{
    if (mMeshOverCapacityLogged)
    {
        DIA_LOG_WARNING("graphics3d", "Mesh3DFrameData: mesh draw budget recovered — no drops this frame.");
        mMeshOverCapacityLogged = false;
    }
    if (mDirectionalLightOverCapacityLogged)
    {
        DIA_LOG_WARNING("graphics3d", "Mesh3DFrameData: directional light budget recovered — no drops this frame.");
        mDirectionalLightOverCapacityLogged = false;
    }
    if (mPointLightOverCapacityLogged)
    {
        DIA_LOG_WARNING("graphics3d", "Mesh3DFrameData: point light budget recovered — no drops this frame.");
        mPointLightOverCapacityLogged = false;
    }
    mMeshDraws.RemoveAll();
    mDirectionalLights.RemoveAll();
    mPointLights.RemoveAll();
    mAmbientLight = AmbientLight{};
    mDroppedMeshes = 0;
    mDroppedLights = 0;
}

////////////////////////////////////////////////////////////
void Mesh3DFrameData::Copy(const Mesh3DFrameData& rhs)
{
    mCamera            = rhs.mCamera;
    mAmbientLight      = rhs.mAmbientLight;
    mMeshDraws.Assign(rhs.mMeshDraws);
    mDirectionalLights.Assign(rhs.mDirectionalLights);
    mPointLights.Assign(rhs.mPointLights);
    mDroppedMeshes     = rhs.mDroppedMeshes;
    mDroppedLights     = rhs.mDroppedLights;
}

////////////////////////////////////////////////////////////
const Camera3D& Mesh3DFrameData::GetCamera() const
{
    return mCamera;
}

////////////////////////////////////////////////////////////
const AmbientLight& Mesh3DFrameData::GetAmbientLight() const
{
    return mAmbientLight;
}

////////////////////////////////////////////////////////////
const Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand, Mesh3DFrameData::kMaxMeshDraws>& Mesh3DFrameData::GetMeshDraws() const
{
    return mMeshDraws;
}

////////////////////////////////////////////////////////////
const Dia::Core::Containers::DynamicArrayC<DirectionalLight, Mesh3DFrameData::kMaxLights>& Mesh3DFrameData::GetDirectionalLights() const
{
    return mDirectionalLights;
}

////////////////////////////////////////////////////////////
const Dia::Core::Containers::DynamicArrayC<PointLight, Mesh3DFrameData::kMaxLights>& Mesh3DFrameData::GetPointLights() const
{
    return mPointLights;
}

////////////////////////////////////////////////////////////
uint32_t Mesh3DFrameData::DroppedMeshCount() const
{
    return mDroppedMeshes;
}

////////////////////////////////////////////////////////////
uint32_t Mesh3DFrameData::DroppedLightCount() const
{
    return mDroppedLights;
}

} }
