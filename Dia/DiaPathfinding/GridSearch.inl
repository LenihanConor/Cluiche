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
}
