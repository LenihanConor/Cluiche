#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow {

    enum class StreamKind { kFrame, kEvent };

    // IStreamStore
    // ---------------------------------------------------------------------------
    // Type-erased base for FrameStreamStore<T> and EventStreamStore<T>.
    // Application owns stores via UniquePtr<IStreamStore> and looks them up by
    // StringCRC id. Typed handles static_cast back to their concrete type.
    // ---------------------------------------------------------------------------
    class IStreamStore
    {
    public:
        virtual ~IStreamStore() = default;
        virtual const Dia::Core::StringCRC& GetId() const = 0;
        virtual StreamKind GetKind() const = 0;
    };

}} // namespace Dia::ApplicationFlow
