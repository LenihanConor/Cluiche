#pragma once

#include "DiaCore/CRC/StringCRC.h"

namespace Dia::SoftBody2D {

enum class BodyType { kRope, kCloth };

class SoftBody {
public:
    virtual ~SoftBody() = default;
    virtual const Dia::Core::StringCRC& GetId() const = 0;
    virtual BodyType GetBodyType() const = 0;
};

} // namespace Dia::SoftBody2D
