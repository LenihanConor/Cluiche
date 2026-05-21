#include <DiaMailbox/Mailbox.h>
#include <DiaCore/Core/Log.h>

namespace Dia::Mailbox {

    Mailbox::Mailbox()
        : mWarnFn(nullptr)
    {
    }

    Mailbox::~Mailbox() {
        // Destruct all live slots and free each descriptor
        for (uint32_t i = 0; i < mRegistry.Size(); ++i) {
            TypedQueueDescriptor* desc = mRegistry[i];
            if (desc == nullptr) { continue; }

            // Destruct all live T and Address objects in the ring.
            // destructFn handles both T and Address (see SlotDestruct<T>).
            for (uint32_t j = 0; j < desc->count; ++j) {
                const uint32_t slotIdx = (desc->head + j) % desc->capacity;
                uint8_t* slot = desc->slotBuffer + (slotIdx * desc->slotStride);
                desc->destructFn(slot, desc->tOffset);
            }

            delete[] desc->slotBuffer;
            desc->slotBuffer = nullptr;
            delete desc;
        }
    }

    void Mailbox::SetWarnCallback(void(*fn)(const char*)) {
        mWarnFn = fn;
    }

    Mailbox::TypedQueueDescriptor* Mailbox::FindDescriptor(uint32_t key) {
        for (uint32_t i = 0; i < mRegistry.Size(); ++i) {
            if (mRegistry[i]->typeKey == key) {
                return mRegistry[i];
            }
        }
        return nullptr;
    }

    void Mailbox::EmitWarning(const char* msg) {
        if (mWarnFn != nullptr) {
            mWarnFn(msg);
        } else {
            Dia::Core::Log::OutputLine(msg);
        }
    }

} // namespace Dia::Mailbox
