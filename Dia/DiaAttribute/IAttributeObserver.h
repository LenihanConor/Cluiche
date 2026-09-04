#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Entity.h>

namespace Dia::Attribute {

    struct AttributeChangedEvent {
        Dia::Entity::Entity   entity;
        Dia::Core::StringCRC  attribute_name;
        float                 old_value;
        float                 new_value;
    };

    class IAttributeObserver {
    public:
        virtual ~IAttributeObserver() = default;
        virtual void OnAttributeChanged        (const AttributeChangedEvent&) {}
        virtual void OnAttributeReachedMaximum (Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name) {}
        virtual void OnAttributeReachedMinimum (Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name) {}
    };

} // namespace Dia::Attribute
