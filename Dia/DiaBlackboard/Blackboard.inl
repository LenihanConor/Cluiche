#pragma once

#include "DiaCore/Core/Assert.h"
#include "DiaBlackboard/Blackboard.h"

namespace Dia { namespace Blackboard {

    namespace Detail
    {
        // Returns a unique tag pointer per type T without requiring RTTI.
        // Each instantiation produces a distinct function-local static, so
        // &tag is unique per T and stable for the lifetime of the process.
        template<typename T>
        inline void* TypeTag()
        {
            static char tag = 0;
            return &tag;
        }
    }

    struct Blackboard::SlotEntry
    {
        Dia::Core::StringCRC key;
        void*                typeTag;     // == Detail::TypeTag<T>() — no RTTI needed
        void*                data;
        void(*destructor)(void*);

        SlotEntry()
            : key()
            , typeTag(nullptr)
            , data(nullptr)
            , destructor(nullptr)
        {}
    };

    template<typename T>
    T& Blackboard::Register(Dia::Core::StringCRC key)
    {
        DIA_ASSERT(FindSlotIndex(key) == -1, "Blackboard slot already registered");
        DIA_ASSERT(!mSlots.IsFull(), "Blackboard slot capacity exceeded");

        SlotEntry* entry  = new SlotEntry();
        entry->key        = key;
        entry->typeTag    = Detail::TypeTag<T>();
        entry->data       = new T();
        entry->destructor = [](void* p) { delete static_cast<T*>(p); };

        mSlots.Add(entry);

        NotifyRegistered(key);

        return *static_cast<T*>(entry->data);
    }

    template<typename T>
    T& Blackboard::Get(Dia::Core::StringCRC key)
    {
        int index = FindSlotIndex(key);
        DIA_ASSERT(index != -1, "Blackboard slot not found");
        SlotEntry* entry = mSlots[static_cast<unsigned int>(index)];
        DIA_ASSERT(entry->typeTag == Detail::TypeTag<T>(), "Blackboard slot type mismatch");
        return *static_cast<T*>(entry->data);
    }

    template<typename T>
    const T& Blackboard::Get(Dia::Core::StringCRC key) const
    {
        int index = FindSlotIndex(key);
        DIA_ASSERT(index != -1, "Blackboard slot not found");
        const SlotEntry* entry = mSlots[static_cast<unsigned int>(index)];
        DIA_ASSERT(entry->typeTag == Detail::TypeTag<T>(), "Blackboard slot type mismatch");
        return *static_cast<const T*>(entry->data);
    }

    template<typename T>
    T* Blackboard::TryGet(Dia::Core::StringCRC key)
    {
        int index = FindSlotIndex(key);
        if (index == -1)
            return nullptr;
        SlotEntry* entry = mSlots[static_cast<unsigned int>(index)];
        if (entry->typeTag != Detail::TypeTag<T>())
            return nullptr;
        return static_cast<T*>(entry->data);
    }

    template<typename T>
    const T* Blackboard::TryGet(Dia::Core::StringCRC key) const
    {
        int index = FindSlotIndex(key);
        if (index == -1)
            return nullptr;
        const SlotEntry* entry = mSlots[static_cast<unsigned int>(index)];
        if (entry->typeTag != Detail::TypeTag<T>())
            return nullptr;
        return static_cast<const T*>(entry->data);
    }

}}
