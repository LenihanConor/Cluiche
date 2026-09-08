////////////////////////////////////////////////////////////////////////////////
// Filename: FrameCaptureWriter.cpp
////////////////////////////////////////////////////////////////////////////////
#include "FrameCaptureWriter.h"

#include <stb_image_write.h>
#include <cstring>
#include <thread>

namespace Dia
{
    namespace Bgfx
    {

        void FrameCaptureWriter::WriteAsync(const Dia::Graphics::FrameCaptureResult& result,
                                            const char* path)
        {
            if (result.status != Dia::Graphics::FrameCaptureResult::Status::kReady)
                return;

            if (result.data == nullptr || path == nullptr)
                return;

            const unsigned int dataSize = result.pitch * result.height;
            unsigned char* copy = new unsigned char[dataSize];
            std::memcpy(copy, result.data, dataSize);

            // Capture by value so the thread owns all data; path is a raw char*
            // — caller guarantees lifetime for the duration of the async write.
            const int w      = static_cast<int>(result.width);
            const int h      = static_cast<int>(result.height);
            const int stride = static_cast<int>(result.pitch);

            std::thread([copy, w, h, stride, path]()
            {
                stbi_write_png(path, w, h, 4, copy, stride);
                delete[] copy;
            }).detach();
        }

        bool FrameCaptureWriter::WriteSync(const Dia::Graphics::FrameCaptureResult& result,
                                           const char* path)
        {
            if (result.status != Dia::Graphics::FrameCaptureResult::Status::kReady)
                return false;

            if (result.data == nullptr || path == nullptr)
                return false;

            const int w      = static_cast<int>(result.width);
            const int h      = static_cast<int>(result.height);
            const int stride = static_cast<int>(result.pitch);

            const int ret = stbi_write_png(path, w, h, 4, result.data, stride);
            return ret != 0;
        }

    } // namespace Bgfx
} // namespace Dia
