#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <typeindex>

namespace Dia { namespace ApplicationFlow {

// ---------------------------------------------------------------------------
// StreamTypeRegistry
//
// Process-static registry mapping C++ type -> StringCRC type ID.
// Populated by DIA_STREAM_TYPE(T) static-init registrations.
// Zero public instance methods — everything is static.
//
// Uses a fixed-capacity array instead of a hash table to avoid
// static-init order issues inherent in non-trivial static objects.
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

private:
    // No instances
    StreamTypeRegistry() = delete;

    struct Entry
    {
        std::type_index typeIdx{ typeid(void) };
        Dia::Core::StringCRC typeId;
    };

    static constexpr unsigned int kMaxTypes = 64;

    // Plain array — safe for static-init, no constructor dependencies.
    static Entry        s_entries[kMaxTypes];
    static unsigned int s_count;
};

// ---------------------------------------------------------------------------
// StreamTypeRegistry::GetTypeId<T>
// ---------------------------------------------------------------------------
template<typename T>
/*static*/ Dia::Core::StringCRC StreamTypeRegistry::GetTypeId()
{
    const std::type_index target{ typeid(T) };
    for (unsigned int i = 0; i < s_count; ++i)
    {
        if (s_entries[i].typeIdx == target)
        {
            return s_entries[i].typeId;
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

}} // namespace Dia::ApplicationFlow
