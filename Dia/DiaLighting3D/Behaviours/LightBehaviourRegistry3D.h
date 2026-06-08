////////////////////////////////////////////////////////////////////////////////
// Filename: LightBehaviourRegistry3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class LightBehaviourRegistry3D
{
public:
    using FactoryFn = ILightBehaviour3D* (*)(const void* config);

    static constexpr unsigned int kMaxBehaviourTypes = 16;

    static LightBehaviourRegistry3D& Get();

    void               Register    (Dia::Core::StringCRC typeId, FactoryFn factory);
    ILightBehaviour3D* Create      (Dia::Core::StringCRC typeId, const void* config = nullptr) const;
    bool               IsRegistered(Dia::Core::StringCRC typeId) const;

private:
    LightBehaviourRegistry3D() = default;

    struct Entry
    {
        Dia::Core::StringCRC typeId;
        FactoryFn            factory = nullptr;
    };

    Entry        mEntries[kMaxBehaviourTypes];
    unsigned int mCount = 0;
};

} }
