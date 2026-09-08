// Dia/DiaCaptureTest/FrameDiff.h
#pragma once
namespace Dia { namespace CaptureTest {

struct RegionStat
{
    unsigned int row;
    unsigned int col;
    float        differingPct;
    unsigned int maxDelta;
};

struct FrameDiffResult
{
    bool         pass;
    unsigned int totalPixels;
    unsigned int differingPixels;
    float        differingPct;
    unsigned int maxDelta;
    unsigned int threshold;

    static constexpr unsigned int kMaxRegions = 16; // 4x4 grid
    RegionStat   regions[kMaxRegions];
    unsigned int regionCount;
};

class FrameDiff
{
public:
    // gridDim: number of rows/cols in the region grid (default 4 -> 4x4)
    static FrameDiffResult Diff(const void* refPixels,
                                const void* runPixels,
                                unsigned int width,
                                unsigned int height,
                                unsigned int threshold,
                                unsigned int gridDim = 4);
};

} }
