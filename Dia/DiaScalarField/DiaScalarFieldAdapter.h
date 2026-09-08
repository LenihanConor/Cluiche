#pragma once
#include <DiaScalarField/IDiaScalarField.h>
#include <DiaScalarField/DiaScalarField.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace Dia
{
    namespace ScalarField
    {

        // --------------------------------------------------------------------
        // DiaScalarFieldAdapter<Topology, Policy>
        //
        // Header-only template adapter that wraps DiaScalarField<Topology, Policy>
        // and implements IDiaScalarField.
        //
        // Internal use of std::vector, std::string, and std::unordered_map is
        // acceptable here — PD-004 only restricts the public API signatures on
        // IDiaScalarField, which this adapter inherits unchanged.
        //
        // History ring buffer (SFI-001 / SFI-002):
        //   - Default depth: 60 frames (configurable at construction).
        //   - mSnapshots stores frame-major float data indexed by the same
        //     BFS cell ordering that DiaScalarField itself uses.
        //   - When the ring is not yet full, mSnapshots[frame] is frame-th
        //     snapshot (frame=0 is oldest).
        //   - Once full, mWriteHead is the next slot to overwrite (i.e. the
        //     oldest entry), so actualIdx = (mWriteHead + frame) % historyDepth.
        // --------------------------------------------------------------------
        template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
        class DiaScalarFieldAdapter : public IDiaScalarField
        {
        public:
            // -----------------------------------------------------------------
            // Construction
            // -----------------------------------------------------------------

            // `field`        — reference to the concrete field; lifetime must
            //                  outlast this adapter.
            // `name`         — display name used by the inspector panel.
            // `historyDepth` — snapshot ring buffer capacity (clamped to >= 1).
            explicit DiaScalarFieldAdapter(DiaScalarField<Topology, Policy>& field,
                                           const char* name,
                                           int historyDepth = 60)
                : mField(field)
                , mName(name)
                , mHistoryDepth(historyDepth > 0 ? historyDepth : 1)
            {}

            // -----------------------------------------------------------------
            // IDiaScalarField — identity + metadata
            // -----------------------------------------------------------------

            const char* GetName()      const override { return mName.c_str(); }
            int         GetCellCount() const override { return mField.GetCellCount(); }

            // -----------------------------------------------------------------
            // IDiaScalarField — live read
            // -----------------------------------------------------------------

            float GetValue(CellIndex cell) const override
            {
                return mField.GetValue(cell);
            }

            // -----------------------------------------------------------------
            // IDiaScalarField — snapshot API
            // -----------------------------------------------------------------

            void CaptureSnapshot() override
            {
                // Build the ordered cell list on first capture.
                if (mCellList.empty())
                    BuildCellList();

                const int count = mField.GetCellCount();
                std::vector<float> frame;
                frame.reserve(static_cast<std::size_t>(count));
                for (const CellIndex& c : mCellList)
                    frame.push_back(mField.GetValue(c));

                if (static_cast<int>(mSnapshots.size()) < mHistoryDepth)
                {
                    // Ring not yet full — just append.
                    mSnapshots.push_back(std::move(frame));
                }
                else
                {
                    // Ring full — overwrite the oldest slot and advance head.
                    mSnapshots[mWriteHead] = std::move(frame);
                    mWriteHead = (mWriteHead + 1) % mHistoryDepth;
                }

                ++mTotalCaptured;
            }

            int GetSnapshotCount() const override
            {
                return static_cast<int>(mSnapshots.size());
            }

            float GetSnapshotValue(int frame, CellIndex cell) const override
            {
                const int snapshotCount = static_cast<int>(mSnapshots.size());
                if (snapshotCount == 0 || frame < 0 || frame >= snapshotCount)
                    return 0.0f;

                int actualIdx;
                if (snapshotCount < mHistoryDepth)
                {
                    // Ring not yet full — snapshots stored sequentially, 0 = oldest.
                    actualIdx = frame;
                }
                else
                {
                    // Ring full — mWriteHead points to the oldest slot.
                    actualIdx = (mWriteHead + frame) % mHistoryDepth;
                }

                const int linearIdx = FindCellLinearIndex(cell);
                if (linearIdx < 0 ||
                    linearIdx >= static_cast<int>(mSnapshots[actualIdx].size()))
                {
                    return 0.0f;
                }
                return mSnapshots[actualIdx][linearIdx];
            }

            // -----------------------------------------------------------------
            // IDiaScalarField — write authoring
            // -----------------------------------------------------------------

            void WritePoint(CellIndex cell, float value) override
            {
                mField.WritePoint(cell, value);
            }

            void WriteRadial(CellIndex center,
                             float radius,
                             float peak,
                             FalloffCurve curve) override
            {
                mField.WriteRadial(center, radius, peak, curve);
            }

            void WriteBox(CellIndex topLeft, int w, int h, float value) override
            {
                mField.WriteBox(topLeft, w, h, value);
            }

        private:
            // -----------------------------------------------------------------
            // Cell ordering helpers
            // -----------------------------------------------------------------

            // BFS from {0,0} — matches DiaScalarField's internal BuildCellIndex
            // ordering exactly (both topologies guarantee {0,0} is in-bounds).
            void BuildCellList()
            {
                const int count = mField.GetCellCount();
                mCellList.reserve(static_cast<std::size_t>(count));
                mCellToLinearIdx.reserve(static_cast<std::size_t>(count));

                mCellList.push_back(CellIndex{0, 0});
                mCellToLinearIdx[CellIndex{0, 0}] = 0;

                std::size_t head = 0;
                while (head < mCellList.size())
                {
                    const CellIndex cur = mCellList[head++];
                    mField.GetTopology().ForEachNeighbour(cur, [&](CellIndex nb)
                    {
                        if (mCellToLinearIdx.find(nb) == mCellToLinearIdx.end())
                        {
                            const int idx = static_cast<int>(mCellList.size());
                            mCellToLinearIdx[nb] = idx;
                            mCellList.push_back(nb);
                        }
                    });
                }
            }

            int FindCellLinearIndex(CellIndex cell) const
            {
                const auto it = mCellToLinearIdx.find(cell);
                if (it == mCellToLinearIdx.end())
                    return -1;
                return it->second;
            }

            // -----------------------------------------------------------------
            // State
            // -----------------------------------------------------------------

            DiaScalarField<Topology, Policy>& mField;
            std::string                       mName;
            int                               mHistoryDepth;

            // Ordered cell list matching DiaScalarField's BFS ordering.
            // Populated lazily on the first CaptureSnapshot() call.
            std::vector<CellIndex>                          mCellList;
            std::unordered_map<CellIndex, int, CellIndexHash> mCellToLinearIdx;

            // Ring buffer: mSnapshots[i] is one frame of GetCellCount() floats.
            // mWriteHead is the next slot to overwrite once the ring is full.
            std::vector<std::vector<float>> mSnapshots;
            int                             mWriteHead     = 0;
            int                             mTotalCaptured = 0;
        };

    } // namespace ScalarField
} // namespace Dia
