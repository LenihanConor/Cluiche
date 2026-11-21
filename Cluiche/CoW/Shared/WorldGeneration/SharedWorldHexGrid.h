// S-1 shared hex grid owner: ONE WorldHexGrid for every cell-layer registrant of the world (S-1, A4, A1).
// DiaCellLayers registrations borrow the CellGridDesc by address, so the grid is re-emplaced in place on a size change
// (same address, new contents) and `Generation()` bumps so consumers can tell.
//
// Who rebuilds: whoever choreographs layer (un)registration around a size change claims the grid with SetDriven(true)
// (S-1's WorldGenViewHost does: it unregisters, flushes, re-emplaces, re-registers). While nobody has claimed it the
// owner (WorldGenerationModule) keeps it in step with the session dimensions itself via EnsureDims.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/consumer-amendments.md (Amendment 3)
#pragma once

#include <Shared/WorldGeneration/WorldHexGrid.h>

#include <cstdint>
#include <optional>

namespace CoW {
namespace WorldGeneration {

class SharedWorldHexGrid
{
public:
    // std::optional-shaped so the view host's existing call sites read unchanged.
    void emplace(GridDimensions dims) { mGrid.emplace(dims); ++mGeneration; }
    void reset() { mGrid.reset(); }
    explicit operator bool() const { return mGrid.has_value(); }
    WorldHexGrid*       operator->()       { return &*mGrid; }
    const WorldHexGrid* operator->() const { return &*mGrid; }
    WorldHexGrid&       operator*()        { return *mGrid; }
    const WorldHexGrid& operator*() const  { return *mGrid; }

    // Null while no grid has been built.
    const WorldHexGrid* Get() const { return mGrid ? &*mGrid : nullptr; }

    // Builds or rebuilds when the dimensions differ. Returns true when the grid was (re)built.
    bool EnsureDims(GridDimensions dims)
    {
        if (mGrid && mGrid->Dimensions().width == dims.width && mGrid->Dimensions().height == dims.height)
            return false;
        emplace(dims);
        return true;
    }

    // Bumps on every (re)build.
    uint32_t Generation() const { return mGeneration; }

    // True while a registrant choreographs rebuilds itself (see the header comment).
    void SetDriven(bool driven) { mDriven = driven; }
    bool IsDriven() const { return mDriven; }

private:
    std::optional<WorldHexGrid> mGrid;
    uint32_t                    mGeneration = 0;
    bool                        mDriven = false;
};

} // namespace WorldGeneration
} // namespace CoW
