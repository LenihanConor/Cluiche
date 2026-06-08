#include "DiaLighting3D/Registry/LightRegistry3D.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>

namespace Dia { namespace Lighting3D {

LightRegistry3D::LightRegistry3D()
{
}

bool LightRegistry3D::RegisterPoint(Dia::Core::StringCRC id, const PointLight3D& light)
{
    if (Has(id))
    {
        DIA_ASSERT(false, "LightRegistry3D::RegisterPoint — duplicate light id");
        return false;
    }
    DIA_ASSERT(mPointCount < kMaxPointLights, "LightRegistry3D::RegisterPoint — capacity exceeded");
    if (mPointCount >= kMaxPointLights)
        return false;

    mPointSlots[mPointCount].id             = id;
    mPointSlots[mPointCount].light          = light;
    mPointSlots[mPointCount].behaviourCount = 0;
    ++mPointCount;
    return true;
}

bool LightRegistry3D::RegisterDirectional(Dia::Core::StringCRC id, const DirectionalLight3D& light)
{
    if (Has(id))
    {
        DIA_ASSERT(false, "LightRegistry3D::RegisterDirectional — duplicate light id");
        return false;
    }
    DIA_ASSERT(mDirectionalCount < kMaxDirectionalLights, "LightRegistry3D::RegisterDirectional — capacity exceeded");
    if (mDirectionalCount >= kMaxDirectionalLights)
        return false;

    mDirectionalSlots[mDirectionalCount].id             = id;
    mDirectionalSlots[mDirectionalCount].light          = light;
    mDirectionalSlots[mDirectionalCount].behaviourCount = 0;
    ++mDirectionalCount;
    return true;
}

bool LightRegistry3D::RegisterSpot(Dia::Core::StringCRC id, const SpotLight3D& light)
{
    if (Has(id))
    {
        DIA_ASSERT(false, "LightRegistry3D::RegisterSpot — duplicate light id");
        return false;
    }
    DIA_ASSERT(mSpotCount < kMaxSpotLights, "LightRegistry3D::RegisterSpot — capacity exceeded");
    if (mSpotCount >= kMaxSpotLights)
        return false;

    mSpotSlots[mSpotCount].id             = id;
    mSpotSlots[mSpotCount].light          = light;
    mSpotSlots[mSpotCount].behaviourCount = 0;
    ++mSpotCount;
    return true;
}

void LightRegistry3D::SetAmbient(const AmbientLight3D& light)
{
    mAmbient = light;
}

void LightRegistry3D::Unregister(Dia::Core::StringCRC id)
{
    {
        const int idx = FindPointIndex(id);
        if (idx >= 0)
        {
            const unsigned int last = mPointCount - 1;
            if (static_cast<unsigned int>(idx) != last)
                mPointSlots[idx] = mPointSlots[last];
            --mPointCount;
            return;
        }
    }
    {
        const int idx = FindDirectionalIndex(id);
        if (idx >= 0)
        {
            const unsigned int last = mDirectionalCount - 1;
            if (static_cast<unsigned int>(idx) != last)
                mDirectionalSlots[idx] = mDirectionalSlots[last];
            --mDirectionalCount;
            return;
        }
    }
    {
        const int idx = FindSpotIndex(id);
        if (idx >= 0)
        {
            const unsigned int last = mSpotCount - 1;
            if (static_cast<unsigned int>(idx) != last)
                mSpotSlots[idx] = mSpotSlots[last];
            --mSpotCount;
            return;
        }
    }
    DIA_ASSERT(false, "LightRegistry3D::Unregister — light not found");
}

bool LightRegistry3D::Has(Dia::Core::StringCRC id) const
{
    return FindPointIndex(id) >= 0 || FindDirectionalIndex(id) >= 0 || FindSpotIndex(id) >= 0;
}

PointLight3D& LightRegistry3D::GetPoint(Dia::Core::StringCRC id)
{
    const int idx = FindPointIndex(id);
    DIA_ASSERT(idx >= 0, "LightRegistry3D::GetPoint — light not found");
    return mPointSlots[idx].light;
}

DirectionalLight3D& LightRegistry3D::GetDirectional(Dia::Core::StringCRC id)
{
    const int idx = FindDirectionalIndex(id);
    DIA_ASSERT(idx >= 0, "LightRegistry3D::GetDirectional — light not found");
    return mDirectionalSlots[idx].light;
}

SpotLight3D& LightRegistry3D::GetSpot(Dia::Core::StringCRC id)
{
    const int idx = FindSpotIndex(id);
    DIA_ASSERT(idx >= 0, "LightRegistry3D::GetSpot — light not found");
    return mSpotSlots[idx].light;
}

AmbientLight3D& LightRegistry3D::GetAmbient()
{
    return mAmbient;
}

unsigned int LightRegistry3D::GetPointCount() const
{
    return mPointCount;
}

const PointLight3D& LightRegistry3D::GetPointByIndex(unsigned int i) const
{
    DIA_ASSERT(i < mPointCount, "LightRegistry3D::GetPointByIndex — index out of range");
    return mPointSlots[i].light;
}

unsigned int LightRegistry3D::GetDirectionalCount() const
{
    return mDirectionalCount;
}

const DirectionalLight3D& LightRegistry3D::GetDirectionalByIndex(unsigned int i) const
{
    DIA_ASSERT(i < mDirectionalCount, "LightRegistry3D::GetDirectionalByIndex — index out of range");
    return mDirectionalSlots[i].light;
}

unsigned int LightRegistry3D::GetSpotCount() const
{
    return mSpotCount;
}

const SpotLight3D& LightRegistry3D::GetSpotByIndex(unsigned int i) const
{
    DIA_ASSERT(i < mSpotCount, "LightRegistry3D::GetSpotByIndex — index out of range");
    return mSpotSlots[i].light;
}

template<typename TSlot, unsigned int N>
void LightRegistry3D::DetachBehaviourFromSlots(TSlot (&slots)[N], unsigned int count,
                                               Dia::Core::StringCRC lightId, Dia::Core::StringCRC typeId)
{
    for (unsigned int i = 0; i < count; ++i)
    {
        if (slots[i].id == lightId)
        {
            for (unsigned int b = 0; b < slots[i].behaviourCount; ++b)
            {
                if (slots[i].behaviours[b]->GetTypeId() == typeId)
                {
                    for (unsigned int k = b; k + 1 < slots[i].behaviourCount; ++k)
                        slots[i].behaviours[k] = slots[i].behaviours[k + 1];
                    slots[i].behaviours[--slots[i].behaviourCount] = nullptr;
                    return;
                }
            }
            return;
        }
    }
}

bool LightRegistry3D::AttachBehaviour(Dia::Core::StringCRC lightId, ILightBehaviour3D* behaviour)
{
    {
        const int idx = FindPointIndex(lightId);
        if (idx >= 0)
        {
            DIA_ASSERT(mPointSlots[idx].behaviourCount < kMaxBehaviours,
                       "LightRegistry3D::AttachBehaviour — behaviour capacity exceeded");
            mPointSlots[idx].behaviours[mPointSlots[idx].behaviourCount++] = behaviour;
            return true;
        }
    }
    {
        const int idx = FindDirectionalIndex(lightId);
        if (idx >= 0)
        {
            DIA_ASSERT(mDirectionalSlots[idx].behaviourCount < kMaxBehaviours,
                       "LightRegistry3D::AttachBehaviour — behaviour capacity exceeded");
            mDirectionalSlots[idx].behaviours[mDirectionalSlots[idx].behaviourCount++] = behaviour;
            return true;
        }
    }
    {
        const int idx = FindSpotIndex(lightId);
        if (idx >= 0)
        {
            DIA_ASSERT(mSpotSlots[idx].behaviourCount < kMaxBehaviours,
                       "LightRegistry3D::AttachBehaviour — behaviour capacity exceeded");
            mSpotSlots[idx].behaviours[mSpotSlots[idx].behaviourCount++] = behaviour;
            return true;
        }
    }
    DIA_ASSERT(false, "LightRegistry3D::AttachBehaviour — light not found");
    return false;
}

void LightRegistry3D::DetachBehaviour(Dia::Core::StringCRC lightId, Dia::Core::StringCRC behaviourTypeId)
{
    if (FindPointIndex(lightId) >= 0)
    {
        DetachBehaviourFromSlots(mPointSlots, mPointCount, lightId, behaviourTypeId);
        return;
    }
    if (FindDirectionalIndex(lightId) >= 0)
    {
        DetachBehaviourFromSlots(mDirectionalSlots, mDirectionalCount, lightId, behaviourTypeId);
        return;
    }
    if (FindSpotIndex(lightId) >= 0)
    {
        DetachBehaviourFromSlots(mSpotSlots, mSpotCount, lightId, behaviourTypeId);
        return;
    }
    DIA_ASSERT(false, "LightRegistry3D::DetachBehaviour — light not found");
}

void LightRegistry3D::UpdateAll(float dt)
{
    for (unsigned int i = 0; i < mPointCount; ++i)
        for (unsigned int b = 0; b < mPointSlots[i].behaviourCount; ++b)
            mPointSlots[i].behaviours[b]->Update(dt);

    for (unsigned int i = 0; i < mDirectionalCount; ++i)
        for (unsigned int b = 0; b < mDirectionalSlots[i].behaviourCount; ++b)
            mDirectionalSlots[i].behaviours[b]->Update(dt);

    for (unsigned int i = 0; i < mSpotCount; ++i)
        for (unsigned int b = 0; b < mSpotSlots[i].behaviourCount; ++b)
            mSpotSlots[i].behaviours[b]->Update(dt);
}

int LightRegistry3D::FindPointIndex(Dia::Core::StringCRC id) const
{
    for (unsigned int i = 0; i < mPointCount; ++i)
    {
        if (mPointSlots[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

int LightRegistry3D::FindDirectionalIndex(Dia::Core::StringCRC id) const
{
    for (unsigned int i = 0; i < mDirectionalCount; ++i)
    {
        if (mDirectionalSlots[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

int LightRegistry3D::FindSpotIndex(Dia::Core::StringCRC id) const
{
    for (unsigned int i = 0; i < mSpotCount; ++i)
    {
        if (mSpotSlots[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

} }
