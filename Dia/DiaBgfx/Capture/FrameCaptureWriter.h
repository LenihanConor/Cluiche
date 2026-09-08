////////////////////////////////////////////////////////////////////////////////
// Filename: FrameCaptureWriter.h
// Writes a FrameCaptureResult to a PNG file using stb_image_write.
// Async variant dispatches to a worker thread (fire-and-forget).
// Sync variant blocks caller until write completes.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Interface/FrameCapture.h>

namespace Dia
{
    namespace Bgfx
    {

        class FrameCaptureWriter
        {
        public:
            // Write result to path asynchronously (fire-and-forget worker thread).
            // Returns immediately. path must be valid for the duration of the write.
            static void WriteAsync(const Dia::Graphics::FrameCaptureResult& result,
                                   const char* path);

            // Synchronous variant — blocks caller until write completes.
            static bool WriteSync(const Dia::Graphics::FrameCaptureResult& result,
                                  const char* path);
        };

    } // namespace Bgfx
} // namespace Dia
