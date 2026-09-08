#pragma once

#include <DiaAttribute/IAttributeObserver.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Attribute {

    class AttributeObserverSubject {
    public:
        void Subscribe  (IAttributeObserver* observer);
        void Unsubscribe(IAttributeObserver* observer);

        void NotifyAttributeChanged       (const AttributeChangedEvent& e);
        void NotifyAttributeReachedMaximum(Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name);
        void NotifyAttributeReachedMinimum(Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name);

    private:
        Dia::Core::Containers::DynamicArrayC<IAttributeObserver*, 16> mObservers;
    };

} // namespace Dia::Attribute
