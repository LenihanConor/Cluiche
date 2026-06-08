////////////////////////////////////////////////////////////////////////////////
// Filename: ILightBehaviour3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class ILightBehaviour3D
{
public:
    virtual ~ILightBehaviour3D() = default;
    virtual Dia::Core::StringCRC GetTypeId() const = 0;
    virtual void Update(float dt) = 0;
};

} }
