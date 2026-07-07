#include "DiaBlackboard/Blackboard.h"
#include "DiaObservation/Log/DiaLog.h"
#include "DiaCore/Core/Assert.h"

namespace Dia { namespace Blackboard {

    Blackboard::Blackboard()
    {
    }

    Blackboard::~Blackboard()
    {
        // Clean up all slots without notifying observers (they may be destroyed already)
        for (unsigned int i = 0; i < mSlots.Size(); ++i)
        {
            SlotEntry* entry = mSlots[i];
            if (entry->data && entry->destructor)
            {
                entry->destructor(entry->data);
                entry->data = nullptr;
            }
            delete entry;
        }
        mSlots.RemoveAll();
    }

    void Blackboard::Unregister(Dia::Core::StringCRC key)
    {
        int index = FindSlotIndex(key);
        DIA_ASSERT(index != -1, "Blackboard slot not found for unregister");

        SlotEntry* entry = mSlots[static_cast<unsigned int>(index)];

        if (entry->data && entry->destructor)
        {
            entry->destructor(entry->data);
            entry->data = nullptr;
        }

        DIA_LOG_INFO("Blackboard", "Unregistered slot: %s", key.AsChar());

        delete entry;
        mSlots.RemoveAt(static_cast<unsigned int>(index));

        NotifyUnregistered(key);
    }

    bool Blackboard::Has(Dia::Core::StringCRC key) const
    {
        return FindSlotIndex(key) != -1;
    }

    void Blackboard::AddObserver(IBlackboardObserver& observer)
    {
        DIA_ASSERT(!mObservers.IsFull(), "Blackboard observer capacity exceeded");
        mObservers.Add(&observer);
    }

    void Blackboard::RemoveObserver(IBlackboardObserver& observer)
    {
        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            if (mObservers[i] == &observer)
            {
                mObservers.RemoveAt(i);
                return;
            }
        }
        DIA_ASSERT(false, "Blackboard observer not found for removal");
    }

    int Blackboard::FindSlotIndex(Dia::Core::StringCRC key) const
    {
        for (unsigned int i = 0; i < mSlots.Size(); ++i)
        {
            if (mSlots[i]->key == key)
                return static_cast<int>(i);
        }
        return -1;
    }

    void Blackboard::NotifyRegistered(Dia::Core::StringCRC key)
    {
        DIA_LOG_INFO("Blackboard", "Registered slot: %s", key.AsChar());

        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            mObservers[i]->OnSlotRegistered(key);
        }
    }

    void Blackboard::NotifyUnregistered(Dia::Core::StringCRC key)
    {
        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            mObservers[i]->OnSlotUnregistered(key);
        }
    }

}}
