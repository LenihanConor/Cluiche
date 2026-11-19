#pragma once
// GridSearch.inl - template definitions for GridSearch.h.

#include "DiaCore/Core/Assert.h"

namespace Dia::Pathfinding {

    template<CIndexGraph G, class Passable>
    void MultiSourceSteps(const G& graph, const StepSeed* seeds, uint32_t seedCount, Passable passable,
                          uint32_t maxSteps, uint32_t* outSteps, uint32_t* outSeedId, GridSearchScratch& scratch)
    {
        DIA_ASSERT(outSteps != nullptr, "MultiSourceSteps requires outSteps");
        const uint32_t cellCount = graph.CellCount();
        for (uint32_t i = 0; i < cellCount; ++i) {
            outSteps[i] = kUnreached;
            if (outSeedId) outSeedId[i] = kUnreached;
        }

        scratch.Reset();
        auto& pending = scratch.PendingSeeds();
        auto& frontier = scratch.Frontier();
        auto& next = scratch.NextFrontier();

        // Collect valid seeds, insertion-sorted (stable) on (startSteps, seedIndex).
        for (uint32_t i = 0; i < seedCount; ++i) {
            const StepSeed& s = seeds[i];
            if (s.cell >= cellCount || s.startSteps > maxSteps || !passable(s.cell)) continue;
            const PendingSeed entry{ s.startSteps, i, s.cell };
            GrowAdd(pending, entry);
            uint32_t j = pending.Size() - 1;
            while (j > 0 && pending[j - 1].startSteps > entry.startSteps) {
                pending[j] = pending[j - 1];
                --j;
            }
            pending[j] = entry;
        }
        const uint32_t pendingCount = pending.Size();
        if (pendingCount == 0) return;

        uint32_t pi = 0;
        uint32_t level = pending[0u].startSteps;
        while (true) {
            frontier.RemoveAll();
            while (pi < pendingCount && pending[pi].startSteps == level) {
                const PendingSeed& p = pending[pi];
                if (outSteps[p.cell] == kUnreached) {
                    outSteps[p.cell] = level;
                    if (outSeedId) outSeedId[p.cell] = seeds[p.seedIndex].id;
                    GrowAdd(frontier, p.cell);
                }
                ++pi;
            }
            for (uint32_t i = 0; i < next.Size(); ++i) GrowAdd(frontier, next[i]);
            next.RemoveAll();

            if (frontier.Size() == 0) {
                if (pi >= pendingCount) break;
                level = pending[pi].startSteps;
                continue;
            }
            if (level >= maxSteps) break;

            for (uint32_t i = 0; i < frontier.Size(); ++i) {
                const uint32_t c = frontier[i];
                const uint32_t srcId = outSeedId ? outSeedId[c] : 0u;
                graph.ForEachNeighbour(c, [&](uint32_t n) {
                    if (outSteps[n] == kUnreached && passable(n)) {
                        outSteps[n] = level + 1;
                        if (outSeedId) outSeedId[n] = srcId;
                        GrowAdd(next, n);
                    }
                });
            }
            ++level;
        }
    }

    template<CIndexGraph G, class InRegion>
    uint32_t FloodRegion(const G& graph, uint32_t start, InRegion inRegion, uint8_t* seen,
                         uint32_t* outCells, GridSearchScratch&)
    {
        DIA_ASSERT(seen != nullptr && outCells != nullptr, "FloodRegion requires seen and outCells");
        if (start >= graph.CellCount() || seen[start] || !inRegion(start)) return 0;
        uint32_t count = 0;
        outCells[count++] = start;
        seen[start] = 1;
        for (uint32_t head = 0; head < count; ++head) {
            graph.ForEachNeighbour(outCells[head], [&](uint32_t n) {
                if (!seen[n] && inRegion(n)) {
                    seen[n] = 1;
                    outCells[count++] = n;
                }
            });
        }
        return count;
    }

    template<CIndexGraph G, class InRegion>
    uint32_t ConnectedComponents(const G& graph, InRegion inRegion, uint32_t* outLabel,
                                 uint32_t* outSizes, GridSearchScratch& scratch)
    {
        DIA_ASSERT(outLabel != nullptr, "ConnectedComponents requires outLabel");
        const uint32_t cellCount = graph.CellCount();
        for (uint32_t i = 0; i < cellCount; ++i) outLabel[i] = kUnreached;

        scratch.Reset();
        IndexQueue& queue = scratch.Queue();
        uint32_t components = 0;
        for (uint32_t i = 0; i < cellCount; ++i) {
            if (outLabel[i] != kUnreached || !inRegion(i)) continue;
            const uint32_t id = components++;
            uint32_t size = 0;
            outLabel[i] = id;
            queue.Clear();
            queue.Push(i);
            while (!queue.Empty()) {
                const uint32_t c = queue.Pop();
                ++size;
                graph.ForEachNeighbour(c, [&](uint32_t n) {
                    if (outLabel[n] == kUnreached && inRegion(n)) {
                        outLabel[n] = id;
                        queue.Push(n);
                    }
                });
            }
            if (outSizes) outSizes[id] = size;
        }
        return components;
    }

    namespace Detail {
        // Drains the heap held in the scratch. Shared by MultiSourceDijkstra and (Task 7) ContinueDijkstra:
        // it never initialises outputs, so it can continue a search whose state lives in scratch + outputs.
        template<class Cost, CIndexGraph G, class StepCost, class OnSettle>
        void RunDijkstra(const G& graph, StepCost& stepCost, const DijkstraOptions<Cost>& options,
                         GridSearchScratch& scratch, OnSettle& onSettle)
        {
            auto& state = scratch.Dijkstra<Cost>();
            auto& heap = state.heap;
            auto& settled = scratch.Settled();
            Cost* const outCost = options.outCost;
            const bool pushAll = options.pushAll;
            while (!heap.Empty()) {
                const DijkstraEntry<Cost> e = heap.Pop();
                if (settled[e.cell]) continue;                            // stale: already settled
                if (!pushAll && outCost[e.cell] < e.cost) continue;       // stale: a cheaper entry exists
                const Settle verdict = onSettle(e.cell, e.cost, e.seedId);
                if (verdict == Settle::Skip) continue;                    // dropped; cell stays unsettled
                if (verdict == Settle::Park) { GrowAdd(state.parked, e); continue; }
                settled[e.cell] = 1;
                if (pushAll) {   // outputs are written at settle time, before expansion, so stepCost can read them
                    outCost[e.cell] = e.cost;
                    if (options.outPrev) options.outPrev[e.cell] = e.prev;
                    if (options.outSeedId) options.outSeedId[e.cell] = e.seedId;
                }
                if (options.stopAt != nullptr && options.stopAt[e.cell] != 0) {
                    if (options.outStoppedAt) *options.outStoppedAt = e.cell;
                    return;
                }
                graph.ForEachNeighbour(e.cell, [&](uint32_t nb) {
                    if (settled[nb]) return;
                    const Cost step = stepCost(e.cell, nb);
                    if (step < Cost(0)) return;   // negative blocks the edge (PD-008)
                    const Cost cand = e.cost + step;
                    if (pushAll) {
                        heap.Push(DijkstraEntry<Cost>{ cand, nb, e.seedId, e.cell });
                        return;
                    }
                    const bool better = cand < outCost[nb]
                        || (options.outSeedId != nullptr && cand == outCost[nb] && e.seedId < options.outSeedId[nb]);
                    if (better) {
                        outCost[nb] = cand;
                        if (options.outPrev) options.outPrev[nb] = e.cell;
                        if (options.outSeedId) options.outSeedId[nb] = e.seedId;
                        heap.Push(DijkstraEntry<Cost>{ cand, nb, e.seedId, e.cell });
                    }
                });
            }
        }
    }

    template<class Cost, CIndexGraph G, class StepCost, class OnSettle>
    void MultiSourceDijkstra(const G& graph, const CostSeed<Cost>* seeds, uint32_t seedCount, StepCost stepCost,
                             const DijkstraOptions<Cost>& options, GridSearchScratch& scratch, OnSettle onSettle)
    {
        DIA_ASSERT(options.outCost != nullptr, "MultiSourceDijkstra requires options.outCost");
        const uint32_t cellCount = graph.CellCount();
        for (uint32_t i = 0; i < cellCount; ++i) {
            options.outCost[i] = std::numeric_limits<Cost>::infinity();
            if (options.outPrev) options.outPrev[i] = kNoCell;
            if (options.outSeedId) options.outSeedId[i] = kUnreached;
        }
        if (options.outStoppedAt) *options.outStoppedAt = kNoCell;

        scratch.Reset();
        scratch.SetDijkstraRunCostSize(static_cast<uint32_t>(sizeof(Cost)));
        scratch.PrepareSettled(cellCount);
        auto& heap = scratch.Dijkstra<Cost>().heap;
        for (uint32_t i = 0; i < seedCount; ++i) {
            const CostSeed<Cost>& s = seeds[i];
            if (s.cell >= cellCount) continue;
            if (options.pushAll) {   // every seed is queued; outputs are written when it settles
                heap.Push(DijkstraEntry<Cost>{ s.startCost, s.cell, s.id, kNoCell });
                continue;
            }
            if (!(s.startCost < options.outCost[s.cell])) continue;
            options.outCost[s.cell] = s.startCost;
            if (options.outSeedId) options.outSeedId[s.cell] = s.id;
            heap.Push(DijkstraEntry<Cost>{ s.startCost, s.cell, s.id, kNoCell });
        }
        Detail::RunDijkstra<Cost>(graph, stepCost, options, scratch, onSettle);
    }

    template<class Cost>
    uint32_t ResumeParked(GridSearchScratch& scratch)
    {
        DIA_ASSERT(scratch.DijkstraRunCostSize() == sizeof(Cost), "ResumeParked: scratch holds no Dijkstra run of this Cost");
        auto& state = scratch.Dijkstra<Cost>();
        const uint32_t count = state.parked.Size();
        for (uint32_t i = 0; i < count; ++i) state.heap.Push(state.parked[i]);
        state.parked.RemoveAll();
        return count;
    }

    template<class Cost, CIndexGraph G, class StepCost, class OnSettle>
    void ContinueDijkstra(const G& graph, StepCost stepCost, const DijkstraOptions<Cost>& options,
                          GridSearchScratch& scratch, OnSettle onSettle)
    {
        DIA_ASSERT(options.outCost != nullptr, "ContinueDijkstra requires options.outCost");
        DIA_ASSERT(scratch.DijkstraRunCostSize() == sizeof(Cost), "ContinueDijkstra: scratch holds no Dijkstra run of this Cost");
        Detail::RunDijkstra<Cost>(graph, stepCost, options, scratch, onSettle);
    }
}
