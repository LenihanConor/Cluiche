#pragma once

#include <DiaCore/Memory/UniquePtr.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaStreams/OverflowPolicy.h>

namespace Dia { namespace ApplicationFlow {

// IStreamConnector
// ---------------------------------------------------------------------------
// Minimal interface used by stream readers/writers to register a backing
// store during OnConnectStreams(). Implemented by Application.
//
// This interface exists to keep DiaStreams free of a DiaApplicationFlow
// dependency — the stream handle headers (StreamReader, StreamWriter, etc.)
// only need this narrow surface, not the full Application.
// ---------------------------------------------------------------------------
class IStreamConnector
{
public:
    virtual ~IStreamConnector() = default;

    // Register or find an existing stream store by ID. Takes ownership of
    // newStore; if a store with the same ID already exists, returns the
    // existing one and discards the passed-in store. Returns null if the
    // stream ID is not declared in the manifest or capacity is exceeded.
    virtual IStreamStore* RegisterOrFindStreamStore(
        Dia::Core::UniquePtr<IStreamStore> newStore) = 0;

    // Manifest-declared event-stream settings (capacity, overflow policy).
    // Returns true and fills the out-params if the manifest declares the stream;
    // a declared capacity of 0 means "use the default" and is reported as 0.
    // Default implementation reports nothing declared (defaults apply).
    virtual bool GetDeclaredEventStreamConfig(const Dia::Core::StringCRC& /*streamId*/,
                                              unsigned int& /*outCapacity*/,
                                              OverflowPolicy& /*outPolicy*/,
                                              unsigned int& /*outBlockTimeoutMs*/) const
    {
        return false;
    }
};

}} // namespace Dia::ApplicationFlow
