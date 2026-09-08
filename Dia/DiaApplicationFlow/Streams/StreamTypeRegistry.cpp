#include <DiaApplicationFlow/Streams/StreamTypeRegistry.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia { namespace ApplicationFlow {

// ---------------------------------------------------------------------------
// Construct-on-first-use accessors
//
// Using function-local statics avoids the static-initialization-order fiasco.
// When Register() or RegisterSerializer() is called from another TU's static
// init, the arrays are guaranteed to be initialized before being accessed.
// ---------------------------------------------------------------------------
/*static*/ StreamTypeRegistry::Entry* StreamTypeRegistry::Entries()
{
    static Entry s[kMaxTypes]{};
    return s;
}

/*static*/ unsigned int& StreamTypeRegistry::Count()
{
    static unsigned int s = 0;
    return s;
}

/*static*/ StreamTypeRegistry::SerializerEntry* StreamTypeRegistry::Serializers()
{
    static SerializerEntry s[kMaxTypes]{};
    return s;
}

/*static*/ unsigned int& StreamTypeRegistry::SerializerCount()
{
    static unsigned int s = 0;
    return s;
}

// ---------------------------------------------------------------------------
// StreamTypeRegistry::IsRegistered
// ---------------------------------------------------------------------------
/*static*/ bool StreamTypeRegistry::IsRegistered(const Dia::Core::StringCRC& typeId)
{
    const unsigned int count = Count();
    Entry* entries = Entries();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (entries[i].typeId == typeId)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// StreamTypeRegistry::Register
//
// Called from StreamTypeRegistration<T> constructors at static-init time.
// Duplicate registrations (same typeIdx) are silently ignored in Release
// and assert in Debug. Asserts if the table is full.
// ---------------------------------------------------------------------------
/*static*/ void StreamTypeRegistry::Register(const std::type_index& typeIdx,
                                              const Dia::Core::StringCRC& typeId)
{
    unsigned int& count = Count();
    Entry* entries = Entries();

    // Check for duplicate
    for (unsigned int i = 0; i < count; ++i)
    {
        if (entries[i].typeIdx == typeIdx)
        {
            DIA_ASSERT(false,
                "StreamTypeRegistry::Register — type already registered. "
                "DIA_STREAM_TYPE(T) must appear exactly once per type.");
            return; // no-op in Release
        }
    }

    DIA_ASSERT(count < kMaxTypes,
        "StreamTypeRegistry::Register — registry full (kMaxTypes = %u). "
        "Increase StreamTypeRegistry::kMaxTypes.", kMaxTypes);

    if (count < kMaxTypes)
    {
        entries[count].typeIdx = typeIdx;
        entries[count].typeId  = typeId;
        ++count;
    }
}

// ---------------------------------------------------------------------------
// StreamTypeRegistry::RegisterSerializer
//
// Called from StreamTypeSerializerRegistration<T> constructors at
// static-init time. Registers a JSON serializer for the given payload type.
// ---------------------------------------------------------------------------
/*static*/ void StreamTypeRegistry::RegisterSerializer(const Dia::Core::StringCRC& payloadType,
                                                        JsonSerializer fn)
{
    unsigned int& count = SerializerCount();
    SerializerEntry* serializers = Serializers();

    DIA_ASSERT(count < kMaxTypes,
        "StreamTypeRegistry::RegisterSerializer — registry full (kMaxTypes = %u). "
        "Increase StreamTypeRegistry::kMaxTypes.", kMaxTypes);

    if (count < kMaxTypes)
    {
        serializers[count].payloadType    = payloadType;
        serializers[count].fn             = fn;
        serializers[count].warnedMissing  = false;
        ++count;
    }
}

// ---------------------------------------------------------------------------
// StreamTypeRegistry::SerializeToJson
//
// Looks up the serializer for payloadType and invokes it.
// Returns Json::nullValue if no serializer is registered, logging a
// one-time warning per type.
// ---------------------------------------------------------------------------
/*static*/ Json::Value StreamTypeRegistry::SerializeToJson(const Dia::Core::StringCRC& payloadType,
                                                            const void* bytes, size_t size)
{
    const unsigned int count = SerializerCount();
    SerializerEntry* serializers = Serializers();

    // Search registered serializers
    for (unsigned int i = 0; i < count; ++i)
    {
        if (serializers[i].payloadType == payloadType)
        {
            return serializers[i].fn(bytes, size);
        }
    }

    // No serializer found — issue a one-time warning per missing type.
    // Reuse the tail of the serializers array (slots >= count) as
    // miss-tracking entries (fn == nullptr signals a miss-tracking slot).
    for (unsigned int i = count; i < kMaxTypes; ++i)
    {
        if (serializers[i].fn == nullptr && serializers[i].payloadType == payloadType)
        {
            // Already tracked — only warn once
            if (!serializers[i].warnedMissing)
            {
                serializers[i].warnedMissing = true;
                DIA_LOG_WARNING("Application",
                    "StreamTypeRegistry::SerializeToJson — no serializer for type '%s'",
                    payloadType.AsChar());
            }
            return Json::Value(Json::nullValue);
        }
        if (serializers[i].fn == nullptr && serializers[i].payloadType == Dia::Core::StringCRC::kZero)
        {
            // First miss for this type — claim this slot
            serializers[i].payloadType   = payloadType;
            serializers[i].warnedMissing = true;
            DIA_LOG_WARNING("Application",
                "StreamTypeRegistry::SerializeToJson — no serializer for type '%s'",
                payloadType.AsChar());
            return Json::Value(Json::nullValue);
        }
    }

    // Fallback (miss-tracking table full)
    return Json::Value(Json::nullValue);
}

}} // namespace Dia::ApplicationFlow
