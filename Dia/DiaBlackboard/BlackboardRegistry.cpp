#include "DiaBlackboard/BlackboardRegistry.h"
#include "DiaCore/Core/Assert.h"
#include "DiaObservation/Log/DiaLog.h"

namespace Dia { namespace Blackboard {

    const Dia::Core::StringCRC BlackboardRegistry::kLogChannel{"BlackboardInspector"};

    void BlackboardRegistry::Register(Dia::Core::StringCRC id, const char* label, const Blackboard& board)
    {
        DIA_ASSERT(!mEntries.IsFull(), "BlackboardRegistry entry capacity exceeded");

        BlackboardEntry entry;
        entry.id    = id;
        entry.label = label;
        entry.board = &board;
        mEntries.Add(entry);

        DIA_LOG_INFO("BlackboardInspector", "Registered blackboard: %s (%s)", label, id.AsChar());
    }

    void BlackboardRegistry::Unregister(Dia::Core::StringCRC id)
    {
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            if (mEntries[i].id == id)
            {
                DIA_LOG_INFO("BlackboardInspector", "Unregistered blackboard: %s (%s)",
                             mEntries[i].label, id.AsChar());
                mEntries.RemoveAt(i);
                return;
            }
        }
        // Unknown id — no-op per AC3
    }

    const Dia::Core::Containers::DynamicArrayC<BlackboardEntry, 16>& BlackboardRegistry::GetAll() const
    {
        return mEntries;
    }

    int BlackboardRegistry::GetCount() const
    {
        return static_cast<int>(mEntries.Size());
    }

    bool BlackboardRegistry::HasSerializer(const void* typeTag) const
    {
        for (unsigned int i = 0; i < mSerializers.Size(); ++i)
        {
            if (mSerializers[i].typeTag == typeTag)
                return true;
        }
        return false;
    }

    void BlackboardRegistry::Serialize(const void* typeTag, const void* data, Json::Value& out) const
    {
        for (unsigned int i = 0; i < mSerializers.Size(); ++i)
        {
            if (mSerializers[i].typeTag == typeTag)
            {
                mSerializers[i].fn(data, out);
                return;
            }
        }
    }

}}
