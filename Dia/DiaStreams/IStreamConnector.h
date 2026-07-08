#pragma once

#include <DiaCore/Memory/UniquePtr.h>
#include <DiaStreams/IStreamStore.h>

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
};

}} // namespace Dia::ApplicationFlow
