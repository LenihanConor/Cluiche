#pragma once
#include "DiaBlackboard/Blackboard.h"
#include "DiaBlackboard/IBlackboardObserver.h"
#include "DiaCore/Core/Assert.h"

namespace Dia { namespace Blackboard { namespace Testing {

    inline void AssertHasSlot(const Blackboard& board, Dia::Core::StringCRC key)
    {
        DIA_ASSERT(board.Has(key), "Expected slot to be present");
    }

    inline void AssertSlotAbsent(const Blackboard& board, Dia::Core::StringCRC key)
    {
        DIA_ASSERT(!board.Has(key), "Expected slot to be absent");
    }

    class MockBlackboardObserver : public IBlackboardObserver
    {
    public:
        Dia::Core::StringCRC lastRegistered;
        Dia::Core::StringCRC lastUnregistered;
        int registerCount   = 0;
        int unregisterCount = 0;

        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC{"MockObserver"}; }

        void OnSlotRegistered(Dia::Core::StringCRC key) override
        {
            lastRegistered = key;
            registerCount++;
        }

        void OnSlotUnregistered(Dia::Core::StringCRC key) override
        {
            lastUnregistered = key;
            unregisterCount++;
        }
    };

}}}
