#include <DiaAttribute/AttributeObserverSubject.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // Subscribe / Unsubscribe
    // -----------------------------------------------------------------------

    void AttributeObserverSubject::Subscribe(IAttributeObserver* observer)
    {
        DIA_ASSERT(observer != nullptr, "AttributeObserverSubject::Subscribe: observer must not be null");
        if (mObservers.FindIndex(observer) < 0)
        {
            mObservers.Add(observer);
        }
    }

    void AttributeObserverSubject::Unsubscribe(IAttributeObserver* observer)
    {
        mObservers.RemoveFirst(observer);
    }

    // -----------------------------------------------------------------------
    // Notify methods
    // -----------------------------------------------------------------------

    void AttributeObserverSubject::NotifyAttributeChanged(const AttributeChangedEvent& e)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnAttributeChanged(e);
        }
    }

    void AttributeObserverSubject::NotifyAttributeReachedMaximum(Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnAttributeReachedMaximum(entity, attribute_name);
        }
    }

    void AttributeObserverSubject::NotifyAttributeReachedMinimum(Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnAttributeReachedMinimum(entity, attribute_name);
        }
    }

} // namespace Dia::Attribute
