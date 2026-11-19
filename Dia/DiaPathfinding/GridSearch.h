#pragma once
// GridSearch.h - shared vocabulary for the DiaPathfinding grid search helpers.
// Helpers (MultiSourceSteps, FloodRegion, ConnectedComponents, MultiSourceDijkstra,
// PriorityFlood) are added by later tasks and declared in this header.

#include <concepts>
#include <cstdint>

#include "DiaPathfinding/GridSearchScratch.h"

namespace Dia::Pathfinding {
    // Distinct from kNoCell (same value, different meaning): "search never reached this cell".
    constexpr uint32_t kUnreached = 0xFFFFFFFFu;

    // Any graph over dense cell indices 0..CellCount()-1. ForEachNeighbour visits neighbours
    // in a fixed, documented order and accepts any callable taking a uint32_t.
    template<class G>
    concept CIndexGraph = requires(const G& g, uint32_t c) {
        { g.CellCount() } -> std::convertible_to<uint32_t>;
        g.ForEachNeighbour(c, [](uint32_t) {});
    };

    // A MultiSourceSteps seed: `cell` starts at `startSteps`; `id` is reported in outSeedId.
    struct StepSeed {
        uint32_t cell;
        uint32_t startSteps;
        uint32_t id;
    };

    // Bucketed multi-source BFS. steps(cell) = min over seeds of startSteps + passable-path length.
    // outSteps (and outSeedId when non-null) are sized CellCount() and initialised to kUnreached.
    // passable is bool(uint32_t cell). Seeds on impassable / out-of-range cells, or with startSteps >
    // maxSteps, are ignored. maxSteps is inclusive.
    //
    // Order (reproduces a plain pre-seeded FIFO BFS when every startSteps is 0): at each level L the
    // frontier is the seeds with startSteps == L in ascending (startSteps, seed index) order (a seed
    // whose cell is already reached is dropped, so duplicate seeds: lowest startSteps then lowest
    // index wins), followed by the cells discovered from level L-1 in FIFO order of their discoverers.
    // Neighbours are visited in the graph's ForEachNeighbour order and the first discoverer wins, so
    // outSeedId is the nearest seed, ties to the earlier frontier entry (lower seed index). A cell is
    // never overwritten once reached. outSeedId stores StepSeed::id. The first call may grow scratch;
    // a second same-size call does not allocate.
    template<CIndexGraph G, class Passable>
    void MultiSourceSteps(const G& graph, const StepSeed* seeds, uint32_t seedCount, Passable passable,
                          uint32_t maxSteps, uint32_t* outSteps, uint32_t* outSeedId, GridSearchScratch& scratch);
}

#include "DiaPathfinding/GridSearch.inl"
