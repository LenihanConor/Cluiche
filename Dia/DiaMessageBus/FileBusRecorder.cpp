#ifdef DIA_DEBUG

#include <DiaMessageBus/FileBusRecorder.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstdio>

namespace Dia::MessageBus {

    FileBusRecorder::FileBusRecorder()
        : mFile(nullptr), mBuffer(nullptr), mBufferSize(0), mBufferUsed(0), mDisabled(false) {}

    FileBusRecorder::~FileBusRecorder() {
        Close();
    }

    bool FileBusRecorder::Open(const char* path, const Config& config) {
        Close();
        mDisabled = false;

        if (!config.tickSource) {
            Fail("no tick source supplied");
            return false;
        }
        if (!path) {
            Fail("null path");
            return false;
        }

        std::FILE* fp = std::fopen(path, "wb");
        if (!fp) {
            Fail("could not open file for writing");
            return false;
        }

        mConfig = config;
        mBufferSize = config.bufferSizeBytes > 0 ? config.bufferSizeBytes : 1;
        mBuffer = new uint8_t[mBufferSize];
        mBufferUsed = 0;
        mFile = fp;

        WriteBytes("DBTR", 4);
        WriteU32(1);
        if (mDisabled) {
            // Fail() already logged and disabled; release resources.
            Close();
            mDisabled = true;
            return false;
        }
        return true;
    }

    void FileBusRecorder::Flush() {
        if (!mFile || mDisabled) return;
        if (FlushBuffer()) {
            if (std::fflush(static_cast<std::FILE*>(mFile)) != 0) Fail("flush failed");
        }
    }

    void FileBusRecorder::Close() {
        if (mFile) {
            if (!mDisabled) FlushBuffer();
            std::fclose(static_cast<std::FILE*>(mFile));
            mFile = nullptr;
        }
        delete[] mBuffer;
        mBuffer = nullptr;
        mBufferSize = 0;
        mBufferUsed = 0;
    }

    void FileBusRecorder::OnDispatch(Dia::Core::StringCRC typeId,
                                     Dia::Core::StringCRC routerId,
                                     Pass pass,
                                     const void* payload,
                                     uint32_t payloadSize) {
        if (mDisabled || !mFile) return;

        WriteU64(mConfig.tickSource(mConfig.tickContext));
        WriteU32(typeId.Value());
        WriteU32(routerId.Value());
        const uint8_t tail[4] = { static_cast<uint8_t>(pass), 0, 0, 0 };
        WriteBytes(tail, 4);
        WriteU32(payloadSize);
        if (payloadSize > 0 && payload) WriteBytes(payload, payloadSize);
    }

    void FileBusRecorder::WriteBytes(const void* data, uint32_t size) {
        if (mDisabled || !mFile) return;
        const uint8_t* src = static_cast<const uint8_t*>(data);
        while (size > 0) {
            if (mBufferUsed == mBufferSize && !FlushBuffer()) return;
            uint32_t room = mBufferSize - mBufferUsed;
            uint32_t n = size < room ? size : room;
            for (uint32_t i = 0; i < n; ++i) mBuffer[mBufferUsed + i] = src[i];
            mBufferUsed += n;
            src += n;
            size -= n;
        }
    }

    void FileBusRecorder::WriteU32(uint32_t v) {
        const uint8_t b[4] = {
            static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
            static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24) };
        WriteBytes(b, 4);
    }

    void FileBusRecorder::WriteU64(uint64_t v) {
        WriteU32(static_cast<uint32_t>(v));
        WriteU32(static_cast<uint32_t>(v >> 32));
    }

    bool FileBusRecorder::FlushBuffer() {
        if (mBufferUsed == 0) return true;
        const size_t written = std::fwrite(mBuffer, 1, mBufferUsed, static_cast<std::FILE*>(mFile));
        if (written != mBufferUsed) {
            Fail("write failed");
            return false;
        }
        mBufferUsed = 0;
        return true;
    }

    void FileBusRecorder::Fail(const char* reason) {
        if (mDisabled) return;
        mDisabled = true;
        DIA_LOG_WARNING("DiaMessageBus", "FileBusRecorder disabled: %s", reason);
    }

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
