#pragma once

namespace Dia::MessageBus {

    class Bus;

    // Implement and register (via Bus::RegisterFlushAdapter) to have Flush()
    // called once per tick in the pre-Primary step, in registration order,
    // before the Primary pass drains any type queues.
    class IFlushAdapter {
    public:
        virtual ~IFlushAdapter() = default;

        // Drain whatever internal event source this adapter wraps (physics,
        // input, ...) into the bus via Post<T>/Broadcast<T>.
        virtual void Flush(Bus& bus) = 0;
    };

} // namespace Dia::MessageBus
