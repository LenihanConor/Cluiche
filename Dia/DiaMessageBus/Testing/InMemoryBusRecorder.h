#pragma once

#ifdef DIA_DEBUG

#include <vector>
#include <cstdint>
#include <cstring>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/IBusRecorder.h>
#include <DiaMessageBus/BusTypes.h>

namespace Dia::MessageBus::Testing {

    // In-memory test recorder for Bus traffic. Records every OnDispatch call
    // into a std::vector. Suitable for unit tests; not for production recording
    // (use FileBusRecorder for that).
    //
    // Tick source is injectable via function pointer + context. If no tick
    // function is provided, all records have tick=0.
    class InMemoryBusRecorder : public IBusRecorder {
    public:
        struct Record {
            uint64_t                 tick;
            Dia::Core::StringCRC     typeId;
            Dia::Core::StringCRC     routerId;
            Pass                     pass;
            uint32_t                 payloadSize;
            std::vector<uint8_t>     payloadBytes;
        };

        using TickFn = uint64_t (*)(void*);

        // Default constructor: no tick function, tick=0 for all records.
        InMemoryBusRecorder()
            : mTickFn(nullptr), mTickContext(nullptr) {}

        // Constructor with injected tick function.
        InMemoryBusRecorder(TickFn tickFn, void* tickContext)
            : mTickFn(tickFn), mTickContext(tickContext) {}

        ~InMemoryBusRecorder() override = default;

        void OnDispatch(Dia::Core::StringCRC typeId,
                        Dia::Core::StringCRC routerId,
                        Pass pass,
                        const void* payload,
                        uint32_t payloadSize) override {
            Record rec;
            rec.tick = mTickFn ? mTickFn(mTickContext) : 0;
            rec.typeId = typeId;
            rec.routerId = routerId;
            rec.pass = pass;
            rec.payloadSize = payloadSize;
            rec.payloadBytes.resize(payloadSize);
            std::memcpy(rec.payloadBytes.data(), payload, payloadSize);
            mRecords.push_back(rec);
        }

        const std::vector<Record>& GetRecords() const { return mRecords; }

        void Clear() { mRecords.clear(); }

    private:
        TickFn               mTickFn;
        void*                mTickContext;
        std::vector<Record>  mRecords;
    };

} // namespace Dia::MessageBus::Testing

#endif // DIA_DEBUG
