#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include <stdint.h>
#include "DiaCore/Containers/Arrays/DynamicArray.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaBlackboard/IBlackboardObserver.h"

namespace Dia { namespace Blackboard {

    class Blackboard
    {
    public:
        explicit Blackboard(uint32_t maxSlots = 32, Dia::Core::StringCRC name = Dia::Core::StringCRC());
        ~Blackboard();

        Dia::Core::StringCRC GetName() const;

        // Parent chain. Returns false (board unchanged) on cycle or if chain length
        // from this upward would exceed kMaxParentDepth. nullptr detaches.
        // Get/TryGet/Has remain local-only. A parent must outlive its children.
        bool SetParent(const Blackboard* parent);
        const Blackboard* GetParent() const;

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

        static const unsigned int kMaxObservers = 8;
        static const uint32_t kMaxParentDepth = 8;

        uint32_t                                              mMaxSlots;
        Dia::Core::StringCRC                                  mName;
        const Blackboard*                                     mParent = nullptr;
        mutable uint32_t                                      mChildCount = 0;
        Dia::Core::Containers::DynamicArray<SlotEntry*>       mSlots;
        Dia::Core::Containers::DynamicArrayC<IBlackboardObserver*, kMaxObservers> mObservers;
    };

}}

#include "DiaBlackboard/Blackboard.inl"
