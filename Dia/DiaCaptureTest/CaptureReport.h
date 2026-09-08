// Dia/DiaCaptureTest/CaptureReport.h
#pragma once
#include <DiaCaptureTest/FrameDiff.h>

namespace Dia { namespace CaptureTest {

struct CaptureMetadata
{
    const char*  tag;          // stage name / capture tag
    unsigned int frameNumber;
    const char*  backend;      // "dx11", "dx12", "vulkan"
    const char*  commitHash;   // 8-char git short hash, or ""
};

class CaptureReportWriter
{
public:
    // Writes JSON report to path. Returns false on IO error.
    static bool Write(const char*           path,
                      const CaptureMetadata& meta,
                      const FrameDiffResult& diff);
};

} }
