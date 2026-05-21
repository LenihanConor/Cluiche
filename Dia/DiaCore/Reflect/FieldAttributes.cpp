#include "DiaCore/Reflect/FieldAttributes.h"

namespace Dia::Reflect {

FieldAttributeRegistry& FieldAttributeRegistry::Instance() {
    static FieldAttributeRegistry r;
    return r;
}

} // namespace Dia::Reflect
