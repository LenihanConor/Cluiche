#pragma once
#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/DiaScalarField.h>  // for FalloffCurve

namespace Dia
{
    namespace ScalarField
    {

        // --------------------------------------------------------------------
        // IDiaScalarField
        //
        // Type-erased virtual wrapper for DiaScalarField<T,P> instances.
        //
        // Allows the inspector (and any other consumer) to hold heterogeneous
        // field references in a single container without knowing the concrete
        // Topology or Policy types (spec decision SFD-002).
        //
        // The history ring buffer lives here (on the field wrapper), not on
        // the inspector panel, so snapshots are captured continuously even
        // when the panel is closed (spec decision SFI-001).
        //
        // PD-004: No STL containers in the public API. All parameters use
        // only CellIndex and FalloffCurve (both Dia-native types).
        // --------------------------------------------------------------------
        class IDiaScalarField
        {
        public:
            virtual ~IDiaScalarField() = default;

            // -----------------------------------------------------------------
            // Identity + metadata
            // -----------------------------------------------------------------

            virtual const char* GetName()      const = 0;
            virtual int         GetCellCount() const = 0;

            // -----------------------------------------------------------------
            // Live read
            // -----------------------------------------------------------------

            virtual float GetValue(CellIndex cell) const = 0;

            // -----------------------------------------------------------------
            // Snapshot API
            // SFI-001: Buffer lives on the field wrapper, not the inspector.
            // SFI-002: Default history depth is 60 frames (configurable at
            //          adapter construction).
            // -----------------------------------------------------------------

            // Capture the current field state into the ring buffer.
            virtual void  CaptureSnapshot()                                  = 0;

            // Number of snapshots currently held (0..historyDepth).
            virtual int   GetSnapshotCount()                           const = 0;

            // Return the value of `cell` at snapshot `frame`.
            // frame=0 is the oldest snapshot in the ring.
            // Returns 0.0f for out-of-range frame or unknown cell.
            virtual float GetSnapshotValue(int frame, CellIndex cell)  const = 0;

            // -----------------------------------------------------------------
            // Write authoring (for inspector runtime edits)
            // -----------------------------------------------------------------

            virtual void WritePoint (CellIndex cell,
                                     float value)                                     = 0;

            virtual void WriteRadial(CellIndex center,
                                     float radius,
                                     float peak,
                                     FalloffCurve curve)                              = 0;

            virtual void WriteBox   (CellIndex topLeft,
                                     int w,
                                     int h,
                                     float value)                                     = 0;
        };

    } // namespace ScalarField
} // namespace Dia
