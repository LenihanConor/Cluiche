// Dia/DiaCaptureTest/FrameDiff.cpp
#include <DiaCaptureTest/FrameDiff.h>

namespace Dia { namespace CaptureTest {

FrameDiffResult FrameDiff::Diff(const void* refPixels,
                                 const void* runPixels,
                                 unsigned int width,
                                 unsigned int height,
                                 unsigned int threshold,
                                 unsigned int gridDim)
{
    FrameDiffResult result;
    result.pass           = false;
    result.totalPixels    = width * height;
    result.differingPixels = 0;
    result.differingPct   = 0.0f;
    result.maxDelta       = 0;
    result.threshold      = threshold;
    result.regionCount    = 0;

    // Clamp gridDim to avoid exceeding kMaxRegions
    if (gridDim * gridDim > FrameDiffResult::kMaxRegions)
    {
        gridDim = 4; // fallback to default 4x4
    }

    const unsigned int numRegions = gridDim * gridDim;
    result.regionCount = numRegions;

    // Initialise per-region accumulators
    unsigned int regionDiffering[FrameDiffResult::kMaxRegions] = {};
    unsigned int regionTotal[FrameDiffResult::kMaxRegions]     = {};
    unsigned int regionMaxDelta[FrameDiffResult::kMaxRegions]  = {};

    for (unsigned int r = 0; r < numRegions; ++r)
    {
        result.regions[r].row          = r / gridDim;
        result.regions[r].col          = r % gridDim;
        result.regions[r].differingPct = 0.0f;
        result.regions[r].maxDelta     = 0;
        regionDiffering[r]             = 0;
        regionTotal[r]                 = 0;
        regionMaxDelta[r]              = 0;
    }

    if (refPixels == nullptr || runPixels == nullptr || width == 0 || height == 0)
    {
        result.pass = (result.differingPixels == 0);
        return result;
    }

    const unsigned char* ref = static_cast<const unsigned char*>(refPixels);
    const unsigned char* run = static_cast<const unsigned char*>(runPixels);

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            const unsigned int pixelIdx = (y * width + x) * 4;

            // Per-channel absolute diff for R, G, B (ignore alpha)
            const unsigned int dR = (ref[pixelIdx + 0] > run[pixelIdx + 0])
                                        ? (ref[pixelIdx + 0] - run[pixelIdx + 0])
                                        : (run[pixelIdx + 0] - ref[pixelIdx + 0]);
            const unsigned int dG = (ref[pixelIdx + 1] > run[pixelIdx + 1])
                                        ? (ref[pixelIdx + 1] - run[pixelIdx + 1])
                                        : (run[pixelIdx + 1] - ref[pixelIdx + 1]);
            const unsigned int dB = (ref[pixelIdx + 2] > run[pixelIdx + 2])
                                        ? (ref[pixelIdx + 2] - run[pixelIdx + 2])
                                        : (run[pixelIdx + 2] - ref[pixelIdx + 2]);

            // Max of the 3 channel diffs
            unsigned int delta = dR;
            if (dG > delta) delta = dG;
            if (dB > delta) delta = dB;

            if (delta > result.maxDelta)
            {
                result.maxDelta = delta;
            }

            if (delta > threshold)
            {
                ++result.differingPixels;
            }

            // Determine which region cell this pixel belongs to
            const unsigned int cellRow = (y * gridDim) / height;
            const unsigned int cellCol = (x * gridDim) / width;
            const unsigned int cellIdx = cellRow * gridDim + cellCol;

            if (cellIdx < numRegions)
            {
                ++regionTotal[cellIdx];
                if (delta > threshold)
                {
                    ++regionDiffering[cellIdx];
                }
                if (delta > regionMaxDelta[cellIdx])
                {
                    regionMaxDelta[cellIdx] = delta;
                }
            }
        }
    }

    // Compute overall differing percentage
    if (result.totalPixels > 0)
    {
        result.differingPct = (static_cast<float>(result.differingPixels) /
                               static_cast<float>(result.totalPixels)) * 100.0f;
    }

    // Finalise per-region stats
    for (unsigned int r = 0; r < numRegions; ++r)
    {
        result.regions[r].maxDelta = regionMaxDelta[r];
        if (regionTotal[r] > 0)
        {
            result.regions[r].differingPct =
                (static_cast<float>(regionDiffering[r]) /
                 static_cast<float>(regionTotal[r])) * 100.0f;
        }
        else
        {
            result.regions[r].differingPct = 0.0f;
        }
    }

    result.pass = (result.differingPixels == 0);
    return result;
}

} }
