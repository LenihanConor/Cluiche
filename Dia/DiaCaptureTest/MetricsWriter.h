// Dia/DiaCaptureTest/MetricsWriter.h
#pragma once

namespace Dia { namespace CaptureTest {

struct MetricEntry
{
    const char*  key;
    float        value;
};

class MetricsWriter
{
public:
    // Write metrics to path as JSON. Returns false on IO error.
    static bool Write(const char*       path,
                      const char*       tag,
                      unsigned int      frameNumber,
                      const MetricEntry entries[],
                      unsigned int      entryCount);
};

} }
