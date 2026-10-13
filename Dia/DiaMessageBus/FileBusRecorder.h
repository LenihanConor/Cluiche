#pragma once

#ifdef DIA_DEBUG

#include <cstdint>
#include <DiaMessageBus/IBusRecorder.h>

namespace Dia::Observation::Metric { class Counter; }

namespace Dia::MessageBus {

    // Debug-only recorder that writes every dispatched bus message to a binary
    // file (format v1, little-endian):
    //
    //   Header : magic "DBTR" (4) | version u32 (=1)
    //   Record : tick u64 | typeId u32 | routerId u32 | pass u8 | pad u8[3]
    //            | payloadSize u32 | payload bytes[payloadSize]
    //
    // Writes are buffered. Data is guaranteed on disk after Flush() or Close()
    // (the destructor calls Close()). Not crash-safe by design.
    //
    // On open or write failure a single warning is logged, the recorder disables
    // itself and drops all further records; the bus is never affected.
    // Single-threaded: call only from the thread that drives the bus.
    class FileBusRecorder : public IBusRecorder {
    public:
        struct Config {
            using TickFn = uint64_t (*)(void* context);
            TickFn   tickSource      = nullptr;   // required; Open fails if null
            void*    tickContext     = nullptr;   // passed to tickSource
            uint32_t bufferSizeBytes = 64 * 1024; // write buffer; flushes to disk when full
        };

        FileBusRecorder();
        ~FileBusRecorder() override;

        FileBusRecorder(const FileBusRecorder&) = delete;
        FileBusRecorder& operator=(const FileBusRecorder&) = delete;

        // Opens (truncating) the file and writes the header. Returns false and
        // logs one warning on failure (including a missing tick source).
        bool Open(const char* path, const Config& config);

        // Writes buffered data to disk.
        void Flush();

        // Flushes and closes the file. Safe to call repeatedly.
        void Close();

        bool IsOpen() const { return mFile != nullptr; }

        // True after an open/write failure (until the next successful Open).
        bool IsDisabled() const { return mDisabled; }

        void OnDispatch(Dia::Core::StringCRC typeId,
                        Dia::Core::StringCRC routerId,
                        Pass pass,
                        const void* payload,
                        uint32_t payloadSize) override;

    private:
        void WriteBytes(const void* data, uint32_t size);
        void WriteU32(uint32_t v);
        void WriteU64(uint64_t v);
        bool FlushBuffer();
        void Fail(const char* reason);

        void*     mFile;          // FILE*
        uint8_t*  mBuffer;
        uint32_t  mBufferSize;
        uint32_t  mBufferUsed;
        Config    mConfig;
        bool      mDisabled;
        Dia::Observation::Metric::Counter* mMetricRecorded     = nullptr;
        Dia::Observation::Metric::Counter* mMetricRecordedBytes = nullptr;
        Dia::Observation::Metric::Counter* mMetricDropped      = nullptr;
    };

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
