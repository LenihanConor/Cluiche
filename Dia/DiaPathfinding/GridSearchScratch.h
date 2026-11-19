#pragma once
// GridSearchScratch.h - reusable scratch storage for the grid search helpers (FloodRegion,
// ConnectedComponents, MultiSourceSteps, DijkstraSearch, PriorityFlood).
//
// All storage is DynamicArray based (no STL in the public header, PD-004). DynamicArray never grows
// by itself (Add asserts when full), so every push here grows geometrically through GrowAdd. Capacity
// is retained across Reset(): once warm, a call of the same size allocates nothing.
//
// MultiSourceSteps bucket design: bucket index = steps. A level-synchronous BFS uses a
// current-frontier / next-frontier pair (Frontier / NextFrontier). Seeds with different start offsets
// sit in PendingSeeds, sorted by the helper on (startSteps, seedIndex); at each level the helper
// injects the pending seeds whose startSteps equals the level, in seedIndex order, before expanding.
//
// Dijkstra park/resume: parked entries live in the per-Cost state (parked) and the settled flags live
// in Settled(). Reset() must not be called between ResumeParked and ContinueDijkstra; only
// PrepareSettled() (called at search init) rewrites settled flags.

#include <cstdint>

#include "DiaCore/Containers/Arrays/DynamicArray.h"

namespace Dia::Pathfinding {

    // Append to a DynamicArray, growing (doubling, min 16) when full. Requires trivially copyable T
    // (DynamicArray::Reserve copies with memcpy).
    template<class T>
    inline void GrowAdd(Dia::Core::Containers::DynamicArray<T>& array, const T& value)
    {
        if (array.Size() == array.Capacity())
        {
            const unsigned int cap = array.Capacity();
            array.Reserve(cap < 16u ? 16u : cap * 2u);
        }
        array.Add(value);
    }

    // Grow capacity to at least `capacity` (never shrinks, never reallocates when already large enough).
    template<class T>
    inline void EnsureCapacity(Dia::Core::Containers::DynamicArray<T>& array, uint32_t capacity)
    {
        if (array.Capacity() < capacity)
        {
            array.Reserve(capacity);
        }
    }

    // FIFO queue of cell indices. Head index advances on Pop; Clear resets head and size, keeping capacity.
    class IndexQueue {
    public:
        IndexQueue() : mHead(0) {}
        void Reserve(uint32_t capacity) { EnsureCapacity(mData, capacity); }
        void Push(uint32_t value) { GrowAdd(mData, value); }
        uint32_t Pop() { return mData[mHead++]; }
        uint32_t Front() const { return mData[mHead]; }
        bool Empty() const { return mHead >= mData.Size(); }
        uint32_t Size() const { return mData.Size() - mHead; }
        uint32_t Capacity() const { return mData.Capacity(); }
        void Clear() { mData.RemoveAll(); mHead = 0; }
    private:
        Dia::Core::Containers::DynamicArray<uint32_t> mData;
        uint32_t mHead;
    };

    // Binary min-heap over a DynamicArray. Entry must be trivially copyable and provide a strict
    // total order through `bool operator<(const Entry&) const`.
    template<class Entry>
    class BinaryMinHeap {
    public:
        void Reserve(uint32_t capacity) { EnsureCapacity(mData, capacity); }
        void Push(const Entry& entry)
        {
            GrowAdd(mData, entry);
            uint32_t i = mData.Size() - 1;
            while (i > 0)
            {
                const uint32_t parent = (i - 1) / 2;
                if (!(mData[i] < mData[parent])) break;
                Swap(i, parent);
                i = parent;
            }
        }
        const Entry& Top() const { return mData[0u]; }
        Entry Pop()
        {
            const Entry top = mData[0u];
            const uint32_t last = mData.Size() - 1;
            if (last > 0) { mData[0u] = mData[last]; }
            mData.Remove();
            const uint32_t n = mData.Size();
            uint32_t i = 0;
            while (true)
            {
                const uint32_t l = 2 * i + 1;
                const uint32_t r = l + 1;
                uint32_t best = i;
                if (l < n && mData[l] < mData[best]) best = l;
                if (r < n && mData[r] < mData[best]) best = r;
                if (best == i) break;
                Swap(i, best);
                i = best;
            }
            return top;
        }
        bool Empty() const { return mData.Size() == 0; }
        uint32_t Size() const { return mData.Size(); }
        uint32_t Capacity() const { return mData.Capacity(); }
        void Clear() { mData.RemoveAll(); }
    private:
        void Swap(uint32_t a, uint32_t b) { const Entry t = mData[a]; mData[a] = mData[b]; mData[b] = t; }
        Dia::Core::Containers::DynamicArray<Entry> mData;
    };

    // Dijkstra heap entry; ordering (cost, cell, seedId) is a strict total order over distinct entries
    // (seedId is the third key so equal-(cost, cell) entries from different seeds pop lowest seedId first).
    // `prev` is the settled cell that pushed the entry (0xFFFFFFFF for seeds); it is not part of the order and
    // is only consumed by PushAll mode, which writes outPrev at settle time.
    template<class Cost>
    struct DijkstraEntry {
        Cost cost;
        uint32_t cell;
        uint32_t seedId;
        uint32_t prev = 0xFFFFFFFFu;
        bool operator<(const DijkstraEntry& o) const
        {
            if (cost < o.cost) return true;
            if (o.cost < cost) return false;
            if (cell != o.cell) return cell < o.cell;
            return seedId < o.seedId;
        }
    };

    // Priority-flood heap entry; ordering (level, filled, cell).
    struct FloodEntry {
        float level;
        float filled;
        uint32_t cell;
        bool operator<(const FloodEntry& o) const
        {
            if (level != o.level) return level < o.level;
            if (filled != o.filled) return filled < o.filled;
            return cell < o.cell;
        }
    };

    // A seed waiting to start at `startSteps` (MultiSourceSteps).
    struct PendingSeed {
        uint32_t startSteps;
        uint32_t seedIndex;
        uint32_t cell;
    };

    template<class Cost>
    struct DijkstraScratch {
        BinaryMinHeap<DijkstraEntry<Cost>> heap;
        Dia::Core::Containers::DynamicArray<DijkstraEntry<Cost>> parked;
    };

    class GridSearchScratch {
    public:
        // Pre-size every container for a grid of cellCount cells (grow only, never shrinks).
        void Reserve(uint32_t cellCount);

        // Clear all sizes, keep all capacity. Do NOT call between ResumeParked and ContinueDijkstra.
        void Reset();

        // Size the settled flags to cellCount and zero them (Dijkstra init). Keeps capacity once warm.
        void PrepareSettled(uint32_t cellCount);

        IndexQueue& Queue() { return mQueue; }
        Dia::Core::Containers::DynamicArray<uint32_t>& Frontier() { return mFrontier; }
        Dia::Core::Containers::DynamicArray<uint32_t>& NextFrontier() { return mNextFrontier; }
        Dia::Core::Containers::DynamicArray<PendingSeed>& PendingSeeds() { return mPendingSeeds; }
        BinaryMinHeap<FloodEntry>& FloodHeap() { return mFloodHeap; }
        Dia::Core::Containers::DynamicArray<uint8_t>& Settled() { return mSettled; }

        // Dijkstra state for Cost = float or double.
        template<class Cost> DijkstraScratch<Cost>& Dijkstra();

        // sizeof(Cost) of the Dijkstra run whose state this scratch holds (4 float, 8 double), 0 when none.
        // Set by MultiSourceDijkstra, cleared by Reset(); ContinueDijkstra / ResumeParked assert on it.
        uint32_t DijkstraRunCostSize() const { return mDijkstraRunCostSize; }
        void SetDijkstraRunCostSize(uint32_t size) { mDijkstraRunCostSize = size; }

        // Total reserved element capacity across all containers (for retained-capacity tests).
        uint64_t TotalCapacity() const;

    private:
        IndexQueue mQueue;
        Dia::Core::Containers::DynamicArray<uint32_t> mFrontier;
        Dia::Core::Containers::DynamicArray<uint32_t> mNextFrontier;
        Dia::Core::Containers::DynamicArray<PendingSeed> mPendingSeeds;
        BinaryMinHeap<FloodEntry> mFloodHeap;
        Dia::Core::Containers::DynamicArray<uint8_t> mSettled;
        DijkstraScratch<float> mDijkstraFloat;
        DijkstraScratch<double> mDijkstraDouble;
        uint32_t mDijkstraRunCostSize = 0;
    };

    template<> inline DijkstraScratch<float>& GridSearchScratch::Dijkstra<float>() { return mDijkstraFloat; }
    template<> inline DijkstraScratch<double>& GridSearchScratch::Dijkstra<double>() { return mDijkstraDouble; }

} // namespace Dia::Pathfinding
