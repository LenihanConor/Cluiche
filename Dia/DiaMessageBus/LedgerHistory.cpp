#include <DiaMessageBus/LedgerHistory.h>
#ifdef DIA_DEBUG

namespace Dia::MessageBus {

void LedgerHistory::Push(const LedgerSnapshot& snapshot) {
    if (mCount < kLedgerCapacity) {
        const uint32_t idx = (mHead + mCount) % kLedgerCapacity;
        mSlots[idx] = snapshot;
        ++mCount;
    } else {
        mSlots[mHead] = snapshot;
        mHead = (mHead + 1) % kLedgerCapacity;
    }
}

uint32_t LedgerHistory::Count() const {
    return mCount;
}

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
