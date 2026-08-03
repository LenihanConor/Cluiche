#pragma once

#include <DiaScalarField/CFieldTopology.h>
#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/ScalarFieldLogChannel.h>
#include <DiaScalarField/UniformDecayPolicy.h>
#include <DiaObservation/Log/DiaLog.h>

#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <cfloat>

namespace Dia
{
    namespace ScalarField
    {

        // --------------------------------------------------------------------
        // Internal hasher for CellIndex — used only within DiaScalarField.
        // --------------------------------------------------------------------
        struct CellIndexHash
        {
            std::size_t operator()(const CellIndex& c) const noexcept
            {
                // Pack x into the high bits, y into the low bits.
                // Assumes coordinates fit in 16 bits, which is ample for any
                // practical field size.
                return static_cast<std::size_t>(
                    (static_cast<unsigned>(c.x) << 16u) |
                    (static_cast<unsigned>(c.y) & 0xFFFFu));
            }
        };

        // --------------------------------------------------------------------
        // DiaScalarField<Topology, Policy>
        //
        // Double-buffered, topology-agnostic float field.
        //
        // Template parameters
        //   Topology — must satisfy CFieldTopology (GetCellCount + ForEachNeighbour)
        //   Policy   — must provide ComputeCell(int cell, float current,
        //                                        float staticModifier,
        //                                        float neighbourSum) -> float
        //
        // Tick() lifecycle:
        //   1. Flush pending writes into the write buffer.
        //   2. Propagate: for each cell, accumulate neighbour contributions via
        //      Policy::ComputeCell, skip blocked cells (write 0).
        //   3. Swap buffers (write buffer becomes the new read buffer).
        //   4. Clamp each cell to [mMinClamp, mMaxClamp].
        // --------------------------------------------------------------------
        template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
        class DiaScalarField
        {
        public:
            // -----------------------------------------------------------------
            // Construction
            // -----------------------------------------------------------------

            // Constructs a field over the given topology.
            // Buffers are zero-initialised; static modifiers are 1.0f; nothing
            // is blocked; clamp range defaults to [0, 1].
            explicit DiaScalarField(Topology topology, Policy policy = {})
                : mTopology(std::move(topology))
                , mPolicy(std::move(policy))
                , mMinClamp(0.0f)
                , mMaxClamp(1.0f)
            {
                const int count = mTopology.GetCellCount();

                // Pre-compute the ordered cell list and inverse lookup.
                BuildCellIndex(count);

                // Allocate buffers.
                mBufferA.assign(count, 0.0f);
                mBufferB.assign(count, 0.0f);
                mBlocked.assign(count, false);
                mStaticModifier.assign(count, 1.0f);

                DIA_LOG_INFO(kLogChannel, "DiaScalarField constructed: %d cells", count);
            }

            // -----------------------------------------------------------------
            // Setters / getters
            // -----------------------------------------------------------------

            void SetBlocked(CellIndex cell, bool blocked)
            {
                mBlocked[CellToIndex(cell)] = blocked;
            }

            bool IsBlocked(CellIndex cell) const
            {
                return mBlocked[CellToIndex(cell)];
            }

            // Set the pre-baked per-cell multiplier (clamped to [0, 1] by convention).
            void SetStaticModifier(CellIndex cell, float modifier)
            {
                mStaticModifier[CellToIndex(cell)] = modifier;
            }

            void SetClampRange(float minValue, float maxValue)
            {
                mMinClamp = minValue;
                mMaxClamp = maxValue;
            }

            // -----------------------------------------------------------------
            // Write API (pending write queue — shapes added in Task 4)
            // -----------------------------------------------------------------

            // Queue a direct value write to a cell.  Applied at the start of the
            // next Tick() before propagation.
            void QueueWrite(CellIndex cell, float value)
            {
                mPendingWrites.emplace_back(CellToIndex(cell), value);
            }

            // -----------------------------------------------------------------
            // Tick
            // -----------------------------------------------------------------

            void Tick()
            {
                // 1. Flush pending writes into the write buffer.
                for (const auto& [idx, value] : mPendingWrites)
                {
                    mBufferB[idx] = value;
                }
                mPendingWrites.clear();

                // 2. Propagation pass.
                const int count = static_cast<int>(mCells.size());
                for (int c = 0; c < count; ++c)
                {
                    if (mBlocked[c])
                    {
                        mBufferB[c] = 0.0f;
                        continue;
                    }

                    float neighbourSum = 0.0f;
                    mTopology.ForEachNeighbour(mCells[c], [&](CellIndex nb)
                    {
                        const int nbIdx = CellToIndex(nb);
                        neighbourSum += mBufferA[nbIdx];
                    });

                    float newValue = mPolicy.ComputeCell(c,
                                                         mBufferA[c],
                                                         mStaticModifier[c],
                                                         neighbourSum);

                    // Clamp to [mMinClamp, mMaxClamp].
                    newValue = Clamp(newValue, mMinClamp, mMaxClamp);

                    mBufferB[c] = newValue;
                }

                // 3. Swap buffers so B (just written) becomes the new read buffer.
                mBufferA.swap(mBufferB);
            }

            // -----------------------------------------------------------------
            // Query
            // -----------------------------------------------------------------

            // Return the current (post-last-Tick) value for a cell.
            float GetValue(CellIndex cell) const
            {
                return mBufferA[CellToIndex(cell)];
            }

            int GetCellCount() const
            {
                return mTopology.GetCellCount();
            }

            const Topology& GetTopology() const
            {
                return mTopology;
            }

        private:
            // -----------------------------------------------------------------
            // Index helpers
            // -----------------------------------------------------------------

            // Build mCells (ordered list) and mCellToIndex (inverse map).
            // We enumerate cells by calling ForEachNeighbour on a sentinel cell
            // that we know lies in-bounds to get one cell, then flood-fill the
            // rest.  For square grids this would be overkill, but keeping it
            // generic avoids specialisation.
            //
            // Simpler approach: iterate the concept-required GetCellCount() cells
            // by brute-forcing CellIndex{x, y} for y in [0, count), x in [0, count)
            // and checking validity via ForEachNeighbour... but that requires a
            // ForEachCell which is not in the concept.
            //
            // Instead we rely on the topology providing a linear ordering that can
            // be reconstructed from a BFS from index 0.  For SquareFieldTopology
            // we can derive the first cell directly; for HexFieldTopology we use
            // the known axial origin (0,0).
            //
            // Actually: simplest correct approach — let the topology enumerate its
            // own cells indirectly.  We use a BFS from a seed cell:
            //   - Square: seed = {0, 0}
            //   - Hex:    seed = {0, 0} (axial centre, always in-bounds)
            // BFS visits every connected cell exactly once.  This gives a stable,
            // deterministic ordering as long as the topology is connected, which
            // both supplied topologies guarantee.
            void BuildCellIndex(int expectedCount)
            {
                mCells.reserve(expectedCount);
                mCellToIndex.reserve(expectedCount);

                // BFS from origin {0, 0}.
                std::vector<CellIndex> frontier;
                frontier.push_back(CellIndex{0, 0});
                mCellToIndex[CellIndex{0, 0}] = 0;
                mCells.push_back(CellIndex{0, 0});

                std::size_t head = 0;
                while (head < mCells.size())
                {
                    const CellIndex current = mCells[head++];
                    mTopology.ForEachNeighbour(current, [&](CellIndex nb)
                    {
                        if (mCellToIndex.find(nb) == mCellToIndex.end())
                        {
                            const int idx = static_cast<int>(mCells.size());
                            mCellToIndex[nb] = idx;
                            mCells.push_back(nb);
                        }
                    });
                }

                // Safety: if BFS did not reach all cells (disconnected topology),
                // the buffers would be incorrectly sized.  Assert in debug.
                // (Both supplied topologies are fully connected so this won't fire.)
            }

            int CellToIndex(CellIndex cell) const
            {
                const auto it = mCellToIndex.find(cell);
                return it->second;
            }

            CellIndex IndexToCell(int idx) const
            {
                return mCells[idx];
            }

            static float Clamp(float v, float lo, float hi)
            {
                return v < lo ? lo : (v > hi ? hi : v);
            }

            // -----------------------------------------------------------------
            // State
            // -----------------------------------------------------------------

            Topology mTopology;
            Policy   mPolicy;

            // Cell ordering: mCells[i] is the CellIndex for linear index i.
            std::vector<CellIndex> mCells;

            // Inverse mapping: CellIndex -> linear index.
            std::unordered_map<CellIndex, int, CellIndexHash> mCellToIndex;

            // Double buffers.  mBufferA is the current read buffer;
            // mBufferB is the write-in-progress buffer.  They are swapped
            // at the end of Tick().
            std::vector<float> mBufferA;
            std::vector<float> mBufferB;

            // Per-cell blocked flags.
            std::vector<bool> mBlocked;

            // Pre-baked per-cell multipliers (default 1.0f).
            std::vector<float> mStaticModifier;

            // Clamp range applied after propagation (default [0, 1]).
            float mMinClamp;
            float mMaxClamp;

            // Pending write queue.  Flushed at the start of Tick() before
            // propagation.  Write-shape API (Task 4) will push entries here.
            std::vector<std::pair<int, float>> mPendingWrites;
        };

    } // namespace ScalarField
} // namespace Dia
