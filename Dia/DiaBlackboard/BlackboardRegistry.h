#pragma once

#include "DiaBlackboard/Blackboard.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Json/external/json/json.h"

#include <functional>

namespace Dia { namespace Blackboard {

    using SerializeFn = std::function<void(const void* data, Json::Value& out)>;

    struct BlackboardEntry
    {
        Dia::Core::StringCRC  id;
        const char*           label;   // human-readable owner name
        const Blackboard*     board;   // non-owning ptr; must outlive entry
    };

    class BlackboardRegistry
    {
    public:
        void Register(Dia::Core::StringCRC id, const char* label, const Blackboard& board);
        void Unregister(Dia::Core::StringCRC id);

        const Dia::Core::Containers::DynamicArrayC<BlackboardEntry, 16>& GetAll() const;
        int  GetCount() const;

        template<typename T>
        void RegisterSerializer(SerializeFn fn);

        // Used by BlackboardInspectorSource — not for general use
        bool HasSerializer(const void* typeTag) const;
        void Serialize(const void* typeTag, const void* data, Json::Value& out) const;

    private:
        struct SerializerEntry
        {
            const void* typeTag;
            SerializeFn fn;
        };

        static constexpr unsigned int kMaxEntries     = 16;
        static constexpr unsigned int kMaxSerializers = 32;

        static const Dia::Core::StringCRC kLogChannel;

        Dia::Core::Containers::DynamicArrayC<BlackboardEntry,  kMaxEntries>     mEntries;
        Dia::Core::Containers::DynamicArrayC<SerializerEntry,  kMaxSerializers> mSerializers;
    };

    template<typename T>
    void BlackboardRegistry::RegisterSerializer(SerializeFn fn)
    {
        DIA_ASSERT(!mSerializers.IsFull(), "BlackboardRegistry serializer capacity exceeded");

        SerializerEntry entry;
        entry.typeTag = static_cast<const void*>(Detail::TypeTag<T>());
        entry.fn      = fn;
        mSerializers.Add(entry);
    }

}}
