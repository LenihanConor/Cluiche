#pragma once

#ifdef DIA_DEBUG

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>
#include <diaentitytemplate/Entity.h>

namespace Dia::Entity { class IEntityInspectable; }

namespace Dia::EntityVisualDebugger
{

class EntityPositionHelper
{
public:
    static Dia::Maths::Vector2D GetPosition(
        Dia::Entity::IEntityInspectable& inspectable,
        Dia::Entity::Entity entity,
        Dia::Core::StringCRC positionComponentTypeId);

    static bool GlobMatch(const char* pattern, const char* text);
};

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
