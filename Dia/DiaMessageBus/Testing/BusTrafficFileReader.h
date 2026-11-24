#pragma once

#ifdef DIA_DEBUG

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Dia::MessageBus::Testing {

    // Test/diagnostic reader for files written by FileBusRecorder (format v1).
    // Open() validates magic "DBTR" + version; ReadAll() loads every record.
    // Truncated trailing records cause ReadAll() to fail (records read so far are kept).
    class BusTrafficFileReader {
    public:
        static constexpr uint32_t kSupportedVersion = 1;

        struct Record {
            uint64_t             tick = 0;
            uint32_t             typeId = 0;
            uint32_t             routerId = 0;
            uint8_t              pass = 0;
            std::vector<uint8_t> payload;
        };

        BusTrafficFileReader() = default;
        ~BusTrafficFileReader() { Close(); }
        BusTrafficFileReader(const BusTrafficFileReader&) = delete;
        BusTrafficFileReader& operator=(const BusTrafficFileReader&) = delete;

        // Returns false if the file cannot be opened, magic is wrong, or version unknown.
        bool Open(const char* path) {
            Close();
            mFile = std::fopen(path, "rb");
            if (!mFile) return false;
            char magic[4];
            uint32_t version = 0;
            if (std::fread(magic, 1, 4, mFile) != 4 || std::memcmp(magic, "DBTR", 4) != 0 ||
                !ReadU32(version) || version != kSupportedVersion) {
                Close();
                return false;
            }
            mVersion = version;
            return true;
        }

        // Reads the next record. Returns false at clean EOF or on a truncated record
        // (check IsTruncated() to distinguish).
        bool ReadNext(Record& out) {
            if (!mFile) return false;
            uint8_t first;
            if (std::fread(&first, 1, 1, mFile) != 1) return false; // clean EOF
            std::ungetc(first, mFile);

            uint8_t pad[3];
            uint32_t payloadSize = 0;
            if (!ReadU64(out.tick) || !ReadU32(out.typeId) || !ReadU32(out.routerId) ||
                std::fread(&out.pass, 1, 1, mFile) != 1 || std::fread(pad, 1, 3, mFile) != 3 ||
                !ReadU32(payloadSize)) {
                mTruncated = true;
                return false;
            }
            out.payload.resize(payloadSize);
            if (payloadSize > 0 && std::fread(out.payload.data(), 1, payloadSize, mFile) != payloadSize) {
                mTruncated = true;
                return false;
            }
            return true;
        }

        // Reads all remaining records. Returns false if a truncated record was hit.
        bool ReadAll(std::vector<Record>& out) {
            Record r;
            while (ReadNext(r)) out.push_back(r);
            return !mTruncated;
        }

        bool IsTruncated() const { return mTruncated; }
        uint32_t GetVersion() const { return mVersion; }

        void Close() {
            if (mFile) { std::fclose(mFile); mFile = nullptr; }
        }

    private:
        bool ReadU32(uint32_t& v) {
            uint8_t b[4];
            if (std::fread(b, 1, 4, mFile) != 4) return false;
            v = uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
            return true;
        }
        bool ReadU64(uint64_t& v) {
            uint32_t lo, hi;
            if (!ReadU32(lo) || !ReadU32(hi)) return false;
            v = uint64_t(lo) | (uint64_t(hi) << 32);
            return true;
        }

        std::FILE* mFile = nullptr;
        uint32_t   mVersion = 0;
        bool       mTruncated = false;
    };

} // namespace Dia::MessageBus::Testing

#endif // DIA_DEBUG
