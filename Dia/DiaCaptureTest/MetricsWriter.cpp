// Dia/DiaCaptureTest/MetricsWriter.cpp
#include <DiaCaptureTest/MetricsWriter.h>
#include <cstdio>
#include <cstring>

namespace Dia { namespace CaptureTest {

bool MetricsWriter::Write(const char*       path,
                           const char*       tag,
                           unsigned int      frameNumber,
                           const MetricEntry entries[],
                           unsigned int      entryCount)
{
    if (path == nullptr)
    {
        return false;
    }

    FILE* f = fopen(path, "w");
    if (f == nullptr)
    {
        return false;
    }

    const char* safeTag = (tag != nullptr) ? tag : "";

    fprintf(f, "{\n");
    fprintf(f, "  \"tag\": \"%s\",\n", safeTag);
    fprintf(f, "  \"frame\": %u,\n", frameNumber);
    fprintf(f, "  \"metrics\": {\n");

    for (unsigned int i = 0; i < entryCount; ++i)
    {
        const char* key   = (entries[i].key != nullptr) ? entries[i].key : "";
        const char* comma = (i + 1 < entryCount) ? "," : "";
        fprintf(f, "    \"%s\": %.2f%s\n", key, entries[i].value, comma);
    }

    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

} }
