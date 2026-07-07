#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaBlackboard/IBlackboardObserver.h"

namespace Dia { namespace Blackboard {

    class Blackboard
    {
    public:
        Blackboard();
        ~Blackboard();

        template<typename T>
        T& Register(Dia::Core::StringCRC key);

        void Unregister(Dia::Core::StringCRC key);

        template<typename T>
        T& Get(Dia::Core::StringCRC key);

        template<typename T>
        const T& Get(Dia::Core::StringCRC key) const;

        template<typename T>
        T* TryGet(Dia::Core::StringCRC key);

        template<typename T>
        const T* TryGet(Dia::Core::StringCRC key) const;

        bool Has(Dia::Core::StringCRC key) const;

        void AddObserver(IBlackboardObserver& observer);
        void RemoveObserver(IBlackboardObserver& observer);

        // Visitor iteration — used by inspector sources. No STL in signature (PD-004).
        // fn(Dia::Core::StringCRC key, const void* typeTag, const void* data)
        template<typename Fn>
        void VisitSlots(Fn&& fn) const;

        // fn(const IBlackboardObserver* obs)
        template<typename Fn>
        void VisitObservers(Fn&& fn) const;

    private:
        struct SlotEntry;

        int FindSlotIndex(Dia::Core::StringCRC key) const;
        void NotifyRegistered(Dia::Core::StringCRC key);
        void NotifyUnregistered(Dia::Core::StringCRC key);

        static const unsigned int kMaxSlots     = 32;
        static const unsigned int kMaxObservers = 8;

        Dia::Core::Containers::DynamicArrayC<SlotEntry*, kMaxSlots>      mSlots;
        Dia::Core::Containers::DynamicArrayC<IBlackboardObserver*, kMaxObservers> mObservers;
    };

}}

#include "DiaBlackboard/Blackboard.inl"
