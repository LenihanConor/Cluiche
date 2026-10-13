#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/BusTypes.h>

namespace Dia::MessageBus {

    // Pure interface for recording Bus dispatch events. Caller provides
    // an implementation (e.g. InMemoryBusRecorder, FileBusRecorder) and
    // installs it via Bus::SetRecorder. The bus calls OnDispatch once per
    // dispatched message, before subscribers run.
    //
    // The recorder obtains the tick itself from its own injected tick source;
    // the bus has no opinion on what a "tick" means to the application.
    //
    // Debug-only (guarded by #ifdef DIA_DEBUG in Bus.h / DispatchOne<T>).
    class IBusRecorder {
    public:
        virtual ~IBusRecorder() = default;

        // Called once per dispatched message, before subscribers run.
        // Payload points to sizeof(T) bytes of the message struct.
        virtual void OnDispatch(Dia::Core::StringCRC typeId,
                                Dia::Core::StringCRC routerId,
                                Pass pass,
                                const void* payload,
                                uint32_t payloadSize) = 0;
    };

} // namespace Dia::MessageBus
