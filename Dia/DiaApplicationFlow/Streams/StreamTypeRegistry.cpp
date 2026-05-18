#include <DiaApplicationFlow/Streams/StreamTypeRegistry.h>
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace ApplicationFlow {

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------
StreamTypeRegistry::Entry StreamTypeRegistry::s_entries[StreamTypeRegistry::kMaxTypes]{};
unsigned int              StreamTypeRegistry::s_count = 0;

// ---------------------------------------------------------------------------
// StreamTypeRegistry::IsRegistered
// ---------------------------------------------------------------------------
/*static*/ bool StreamTypeRegistry::IsRegistered(const Dia::Core::StringCRC& typeId)
{
    for (unsigned int i = 0; i < s_count; ++i)
    {
        if (s_entries[i].typeId == typeId)
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
    // Check for duplicate
    for (unsigned int i = 0; i < s_count; ++i)
    {
        if (s_entries[i].typeIdx == typeIdx)
        {
            DIA_ASSERT(false,
                "StreamTypeRegistry::Register — type already registered. "
                "DIA_STREAM_TYPE(T) must appear exactly once per type.");
            return; // no-op in Release
        }
    }

    DIA_ASSERT(s_count < kMaxTypes,
        "StreamTypeRegistry::Register — registry full (kMaxTypes = %u). "
        "Increase StreamTypeRegistry::kMaxTypes.", kMaxTypes);

    if (s_count < kMaxTypes)
    {
        s_entries[s_count].typeIdx = typeIdx;
        s_entries[s_count].typeId  = typeId;
        ++s_count;
    }
}

}} // namespace Dia::ApplicationFlow
