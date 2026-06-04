#ifdef DIA_DEBUG

#include "EntityPositionHelper.h"
#include <diaentitytemplate/IEntityInspectable.h>
#include <diaentitytemplate/Entity.h>

namespace Dia::EntityVisualDebugger
{

Dia::Maths::Vector2D EntityPositionHelper::GetPosition(
    Dia::Entity::IEntityInspectable& inspectable,
    Dia::Entity::Entity entity,
    Dia::Core::StringCRC positionComponentTypeId)
{
    Json::Value xVal, yVal;
    float x = 0.0f, y = 0.0f;

    if (inspectable.ReadField(entity, positionComponentTypeId, "x", xVal))
        x = xVal.asFloat();

    if (inspectable.ReadField(entity, positionComponentTypeId, "y", yVal))
        y = yVal.asFloat();

    return Dia::Maths::Vector2D(x, y);
}

bool EntityPositionHelper::GlobMatch(const char* pattern, const char* text)
{
    while (*pattern && *text)
    {
        if (*pattern == '*')
        {
            ++pattern;
            if (!*pattern) return true;
            while (*text)
            {
                if (GlobMatch(pattern, text)) return true;
                ++text;
            }
            return GlobMatch(pattern, text);
        }
        else if (*pattern == '?' || *pattern == *text)
        {
            ++pattern;
            ++text;
        }
        else
        {
            return false;
        }
    }

    while (*pattern == '*') ++pattern;
    return !*pattern && !*text;
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
