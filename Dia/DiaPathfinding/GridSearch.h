#pragma once
// GridSearch.h - shared vocabulary for the DiaPathfinding grid search helpers.
// Helpers (MultiSourceSteps, FloodRegion, ConnectedComponents, MultiSourceDijkstra,
// PriorityFlood) are added by later tasks and declared in this header.

#include <concepts>
#include <cstdint>
#include <limits>

#include "DiaPathfinding/GridSearchScratch.h"
#include "DiaPathfinding/HexTopology.h"   // kNoCell

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

    // BFS flood from `start` over cells where inRegion(cell) holds (bool(uint32_t)). Writes visited cells
    // to outCells in FIFO order (start first; outCells needs room for the region size, at most
    // CellCount()) and sets seen[cell] = 1 for each; returns the number written. Returns 0 when start is
    // out of range, already seen, or fails inRegion. Cells already seen are never re-entered; neighbours
    // are visited in ForEachNeighbour order. Uses outCells as its own queue (no scratch allocation).
    // Note: S-1's Flood does not test start itself (its callers do); this helper does.
    template<CIndexGraph G, class InRegion>
    uint32_t FloodRegion(const G& graph, uint32_t start, InRegion inRegion, uint8_t* seen,
                         uint32_t* outCells, GridSearchScratch& scratch);

    // Connected components of the region {cell : inRegion(cell)} following ForEachNeighbour edges from
    // cells scanned in ascending index (over a directed graph only forward edges are followed). Returns
    // the component count. outLabel (sized CellCount(), also used as the visited marker) is set to the
    // component id, or kUnreached outside the region; ids ascend with each component's lowest-index
    // cell. outSizes may be null; otherwise it is caller-owned, sized >= the component count (at most
    // CellCount()), and outSizes[label] = cell count. The first call may grow scratch; a warm
    // second same-size call does not allocate.
    template<CIndexGraph G, class InRegion>
    uint32_t ConnectedComponents(const G& graph, InRegion inRegion, uint32_t* outLabel,
                                 uint32_t* outSizes, GridSearchScratch& scratch);

    // Settle hook result, returned by onSettle(cell, cost, seedId) for each non-stale popped entry.
    // Accept: settle the cell and expand it. Skip: drop this entry; the cell is NOT settled and stays
    // claimable by other entries. Park: move the entry (original cost / cell / seedId) to the scratch
    // parked list; the cell is NOT settled. ResumeParked re-queues parked entries.
    enum class Settle : uint8_t { Accept, Skip, Park };

    // Default hook: accept every cell.
    struct AcceptAll {
        Settle operator()(uint32_t, auto, uint32_t) const { return Settle::Accept; }
    };

    // A MultiSourceDijkstra seed: `cell` starts at `startCost`; `id` is reported in outSeedId.
    template<class Cost>
    struct CostSeed {
        uint32_t cell;
        Cost startCost;
        uint32_t id;
    };

    // Outputs and early exit for MultiSourceDijkstra. All arrays are caller-owned and sized CellCount().
    template<class Cost>
    struct DijkstraOptions {
        Cost* outCost = nullptr;               // required; infinity() for unreached cells
        uint32_t* outPrev = nullptr;           // optional; kNoCell for seeds and unreached cells
        uint32_t* outSeedId = nullptr;         // optional; kUnreached for unreached cells
        const uint8_t* stopAt = nullptr;       // optional; stop when the first settled cell has stopAt[c] != 0
        uint32_t* outStoppedAt = nullptr;      // optional; the stopping cell, or kNoCell when nothing stopped
        // Additive option motivated by the RegionPartition capped growth (site 9). false (default): strict
        // relaxation. true (PushAll): no relaxation filter; every neighbour that is not yet settled and
        // whose step cost is >= 0 is pushed at cost + step, every seed is pushed, and outCost / outPrev /
        // outSeedId are written when a cell SETTLES (an entry is stale only when its cell is already
        // settled). A Skipped entry therefore never blocks a later, costlier entry from claiming the cell,
        // and outSeedId[from] is valid inside stepCost(from, to) so a seed-dependent step cost can read it.
        bool pushAll = false;
    };

    // Multi-source Dijkstra. Heap order is the strict total order (cost, cell index, seed id). In strict
    // mode with a non-null outSeedId an equal-cost candidate also relaxes when its seed id is lower than
    // outSeedId[cell] (tie rule: lowest seed id wins). onSettle may return Skip or Park (see Settle). stepCost(from, to)
    // returns Cost; a NEGATIVE value blocks the edge. Costs accumulate as `cost + step` in Cost (no
    // widening or narrowing). Edges relax with strict `<`; outPrev / outSeedId follow the improving
    // relaxation (seed id is inherited from the settled cell). Seeds are pushed in index order and improve
    // a cell only when startCost < outCost[cell] (strict, so the earlier seed wins ties). A heap entry is
    // stale (discarded) when its cell is already settled or its cost exceeds outCost[cell]; a cell is
    // settled when a non-stale entry is popped and onSettle(cell, cost, seedId) returns Accept, and its
    // cost is final from then on. Neighbours are expanded in ForEachNeighbour order, skipping settled ones.
    // When the first settled cell has stopAt[c] != 0 the search returns before expanding it and writes
    // *outStoppedAt = c. The heap and settled flags live in the scratch (not in locals) so the Task 7
    // continuation entry points can resume them. The first call may grow scratch; a warm same-size call
    // does not allocate.
    template<class Cost, CIndexGraph G, class StepCost, class OnSettle = AcceptAll>
    void MultiSourceDijkstra(const G& graph, const CostSeed<Cost>* seeds, uint32_t seedCount, StepCost stepCost,
                             const DijkstraOptions<Cost>& options, GridSearchScratch& scratch, OnSettle onSettle = {});

    // Re-pushes every parked entry (original cost / cell / seedId) onto the heap, empties the parked list and
    // returns how many were re-queued. Requires a prior MultiSourceDijkstra<Cost> on this scratch; do not
    // call scratch.Reset() between this and ContinueDijkstra.
    template<class Cost>
    uint32_t ResumeParked(GridSearchScratch& scratch);

    // Drains the heap WITHOUT re-initialising outputs or settled flags, continuing the run held in the
    // scratch (same graph, same options arrays). onSettle may differ from the original run's. Does nothing
    // when the heap is empty. Asserts that the scratch holds a Dijkstra run of the same Cost.
    template<class Cost, CIndexGraph G, class StepCost, class OnSettle = AcceptAll>
    void ContinueDijkstra(const G& graph, StepCost stepCost, const DijkstraOptions<Cost>& options,
                          GridSearchScratch& scratch, OnSettle onSettle = {});
}

#include "DiaPathfinding/GridSearch.inl"
