#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>
#include <functional>

namespace Dia { namespace ApplicationFlow {

    enum class StreamKind { kFrame, kEvent, kService };

    using TapCallback = std::function<void(const void* eventBytes, unsigned int eventSize,
                                           const Dia::Core::StringCRC& streamId)>;
    struct TapHandle { unsigned int id = 0; };

    // IStreamStore
    // ---------------------------------------------------------------------------
    // Type-erased base for FrameStreamStore<T> and EventStreamStore<T>.
    // Application owns stores via UniquePtr<IStreamStore> and looks them up by
    // StringCRC id. Typed handles static_cast back to their concrete type.
    //
    // Tap API (F4): Taps are called synchronously inside Send(), after reader
    // fan-out.  Forbidden inside a TapCallback: allocation, logging, Send, I/O.
    // Max 10µs per callback (DIA_ASSERT in Debug). Re-entrance into Send() on
    // the same store DIA_ASSERTs.
    // ---------------------------------------------------------------------------
    class IStreamStore
    {
    public:
        virtual ~IStreamStore() = default;
        virtual const Dia::Core::StringCRC& GetId()         const = 0;
        virtual StreamKind                  GetKind()        const = 0;
        virtual const Dia::Core::StringCRC& GetPayloadType() const = 0;
        virtual unsigned int                GetMaxReaders()  const = 0;

        // Tap API — implemented by EventStreamStore; FrameStreamStore no-ops.
        virtual TapHandle    AttachTap(TapCallback /*cb*/)    { return TapHandle{0}; }
        virtual void         DetachTap(TapHandle /*handle*/)  {}
        virtual unsigned int GetTapCount()               const { return 0; }

        // Introspection extras — implemented by EventStreamStore; FrameStreamStore
        // returns safe defaults (kDropOldest, 0, 0).
        virtual OverflowPolicy   GetOverflowPolicy()           const { return OverflowPolicy::kDropOldest; }
        virtual unsigned long long GetLastSequence()           const { return 0; }
        virtual unsigned int     GetRegisteredReaderCount()    const { return 0; }

        // Shutdown notification — unblocks kBlock writers. No-op for FrameStream.
        virtual void NotifyShutdown() {}

        // Frame-batching: flush events produced this tick so consumers see them
        // on the next tick. Implemented by EventStreamStore; no-op otherwise.
        virtual void Flush() {}

        // ServiceStream commit gate — implemented by ServiceStreamStore<T>.
        // FrameStream and EventStreamStore return false / no-op defaults.
        virtual bool IsCommitted()  const { return false; }
        virtual bool IsRegistered() const { return false; }
        virtual void Commit()             {}
        virtual void Reset()              {}
    };

}} // namespace Dia::ApplicationFlow
