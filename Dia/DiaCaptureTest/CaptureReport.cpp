// Dia/DiaCaptureTest/CaptureReport.cpp
#include <DiaCaptureTest/CaptureReport.h>
#include <cstdio>
#include <cstring>

namespace Dia { namespace CaptureTest {

bool CaptureReportWriter::Write(const char*            path,
                                 const CaptureMetadata& meta,
                                 const FrameDiffResult& diff)
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

    // Safely resolve nullable strings
    const char* tag        = (meta.tag        != nullptr) ? meta.tag        : "";
    const char* backend    = (meta.backend    != nullptr) ? meta.backend    : "";
    const char* commitHash = (meta.commitHash != nullptr) ? meta.commitHash : "";

    // Write JSON header
    fprintf(f, "{\n");
    fprintf(f, "  \"tag\": \"%s\",\n",     tag);
    fprintf(f, "  \"frame\": %u,\n",       meta.frameNumber);
    fprintf(f, "  \"backend\": \"%s\",\n", backend);
    fprintf(f, "  \"commit\": \"%s\",\n",  commitHash);
    fprintf(f, "  \"pass\": %s,\n",        diff.pass ? "true" : "false");

    // Write diff object
    fprintf(f, "  \"diff\": {\n");
    fprintf(f, "    \"total_pixels\": %u,\n",      diff.totalPixels);
    fprintf(f, "    \"differing_pixels\": %u,\n",  diff.differingPixels);
    fprintf(f, "    \"differing_pct\": %.2f,\n",   diff.differingPct);
    fprintf(f, "    \"max_delta\": %u,\n",          diff.maxDelta);
    fprintf(f, "    \"threshold\": %u,\n",          diff.threshold);

    // Write regions array
    fprintf(f, "    \"regions\": [\n");
    for (unsigned int i = 0; i < diff.regionCount; ++i)
    {
        const RegionStat& rs = diff.regions[i];
        const char* comma    = (i + 1 < diff.regionCount) ? "," : "";
        fprintf(f,
                "      { \"row\": %u, \"col\": %u, \"differing_pct\": %.2f, \"max_delta\": %u }%s\n",
                rs.row, rs.col, rs.differingPct, rs.maxDelta, comma);
    }
    fprintf(f, "    ]\n");

    // Close diff and root objects
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

} }
