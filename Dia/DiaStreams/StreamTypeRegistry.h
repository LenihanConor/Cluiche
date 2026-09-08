#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>
#include <typeindex>

namespace Dia { namespace ApplicationFlow {

// ---------------------------------------------------------------------------
// StreamTypeRegistry
//
// Process-static registry mapping C++ type -> StringCRC type ID.
// Populated by DIA_STREAM_TYPE(T) static-init registrations.
// Zero public instance methods — everything is static.
//
// Uses function-local statics (construct-on-first-use idiom) to avoid
// the static-initialization-order fiasco that would occur with class-level
// or file-scope static arrays when registrations happen during static init
// of other translation units.
// ---------------------------------------------------------------------------
class StreamTypeRegistry
{
public:
    // Returns the StringCRC registered for T.
    // DIA_ASSERTs in Debug if T has not been registered via DIA_STREAM_TYPE(T).
    template<typename T>
    static Dia::Core::StringCRC GetTypeId();

    // Returns true if a type identified by typeId has been registered.
    static bool IsRegistered(const Dia::Core::StringCRC& typeId);

    // Internal — called by StreamTypeRegistration<T> ctor only.
    // Duplicate registrations (same typeIdx) are a no-op in Release and assert in Debug.
    static void Register(const std::type_index& typeIdx, const Dia::Core::StringCRC& typeId);

    // Serializer registry (added for F4 tap/debug forwarding)
    using JsonSerializer = Json::Value(*)(const void* bytes, size_t size);

    // Internal — called by StreamTypeSerializerRegistration<T> ctor only.
    static void RegisterSerializer(const Dia::Core::StringCRC& payloadType, JsonSerializer fn);

    // Returns Json::nullValue if no serializer registered.
    // Logs a one-time warning per type.
    static Json::Value SerializeToJson(const Dia::Core::StringCRC& payloadType,
                                       const void* bytes, size_t size);

private:
    // No instances
    StreamTypeRegistry() = delete;

    static constexpr unsigned int kMaxTypes = 64;

    struct Entry
    {
        std::type_index typeIdx{ typeid(void) };
        Dia::Core::StringCRC typeId;
    };

    struct SerializerEntry
    {
        Dia::Core::StringCRC payloadType;
        JsonSerializer       fn = nullptr;
        bool                 warnedMissing = false;
    };

    // Construct-on-first-use accessors — safe across all static-init ordering.
    static Entry*        Entries();
    static unsigned int& Count();
    static SerializerEntry* Serializers();
    static unsigned int& SerializerCount();
};

// ---------------------------------------------------------------------------
// StreamTypeRegistry::GetTypeId<T>
// ---------------------------------------------------------------------------
template<typename T>
/*static*/ Dia::Core::StringCRC StreamTypeRegistry::GetTypeId()
{
    const std::type_index target{ typeid(T) };
    const unsigned int count = Count();
    Entry* entries = Entries();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (entries[i].typeIdx == target)
        {
            return entries[i].typeId;
        }
    }
    DIA_ASSERT(false, "StreamTypeRegistry::GetTypeId — type not registered. Use DIA_STREAM_TYPE(T) in the owning .cpp.");
    return Dia::Core::StringCRC::kZero;
}

// ---------------------------------------------------------------------------
// StreamTypeRegistration<T>
//
// Instantiated by DIA_STREAM_TYPE(T). Constructor calls
// StreamTypeRegistry::Register once at static-init time.
// ---------------------------------------------------------------------------
template<typename T>
struct StreamTypeRegistration
{
    explicit StreamTypeRegistration(const Dia::Core::StringCRC& typeId)
    {
        StreamTypeRegistry::Register(std::type_index(typeid(T)), typeId);
    }
};

// ---------------------------------------------------------------------------
// StreamTypeSerializerRegistration<T>
//
// Instantiated by DIA_STREAM_TYPE_WITH_SERIALIZER(T, fn). Constructor calls
// StreamTypeRegistry::RegisterSerializer once at static-init time.
// ---------------------------------------------------------------------------
template<typename T>
struct StreamTypeSerializerRegistration
{
    explicit StreamTypeSerializerRegistration(const Dia::Core::StringCRC& typeId,
                                              StreamTypeRegistry::JsonSerializer fn)
    {
        StreamTypeRegistry::RegisterSerializer(typeId, fn);
    }
};

}} // namespace Dia::ApplicationFlow
