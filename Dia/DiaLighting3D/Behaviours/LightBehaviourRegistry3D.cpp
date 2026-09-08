////////////////////////////////////////////////////////////////////////////////
// Filename: LightBehaviourRegistry3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h"

#include <DiaCore/Core/Assert.h>

namespace Dia { namespace Lighting3D {

LightBehaviourRegistry3D& LightBehaviourRegistry3D::Get()
{
    static LightBehaviourRegistry3D sInstance;
    return sInstance;
}

void LightBehaviourRegistry3D::Register(Dia::Core::StringCRC typeId, FactoryFn factory)
{
    DIA_ASSERT(!IsRegistered(typeId), "LightBehaviourRegistry3D::Register — duplicate typeId");
    DIA_ASSERT(mCount < kMaxBehaviourTypes, "LightBehaviourRegistry3D::Register — capacity exceeded");
    mEntries[mCount].typeId  = typeId;
    mEntries[mCount].factory = factory;
    ++mCount;
}

ILightBehaviour3D* LightBehaviourRegistry3D::Create(Dia::Core::StringCRC typeId, const void* config) const
{
    for (unsigned int i = 0; i < mCount; ++i)
    {
        if (mEntries[i].typeId == typeId)
            return mEntries[i].factory(config);
    }
    DIA_ASSERT(false, "LightBehaviourRegistry3D::Create — typeId not found");
    return nullptr;
}

bool LightBehaviourRegistry3D::IsRegistered(Dia::Core::StringCRC typeId) const
{
    for (unsigned int i = 0; i < mCount; ++i)
    {
        if (mEntries[i].typeId == typeId)
            return true;
    }
    return false;
}

} }
