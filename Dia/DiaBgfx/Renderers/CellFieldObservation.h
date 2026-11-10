////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldObservation.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaObservation/Health/HealthReporterBase.h>

#include <stdint.h>

namespace Dia { namespace Observation { namespace Metric { class Counter; class Gauge; class MetricRegistry; } } }

namespace Dia
{
    namespace Bgfx
    {
        // Metric primitives (owned by the MetricRegistry) the CellField observation publishes to.
        // Any pointer may be null; a null metric is skipped.
        struct CellFieldMetrics
        {
            // Counters: published as the delta of the cumulative stat since the previous Update.
            Dia::Observation::Metric::Counter* uploads = nullptr;               // cellfield.uploads
            Dia::Observation::Metric::Counter* uploadedBytes = nullptr;         // cellfield.uploaded_bytes
            Dia::Observation::Metric::Counter* geometryBuilds = nullptr;        // cellfield.geometry_builds
            Dia::Observation::Metric::Counter* droppedCommands = nullptr;       // cellfield.dropped_commands
            Dia::Observation::Metric::Counter* coalescedCommands = nullptr;     // cellfield.coalesced_commands
            Dia::Observation::Metric::Counter* staleHandles = nullptr;          // cellfield.stale_handles
            Dia::Observation::Metric::Counter* unknownColourMaps = nullptr;     // cellfield.unknown_colour_maps
            Dia::Observation::Metric::Counter* outOfRangeCells = nullptr;       // cellfield.out_of_range_cells
            Dia::Observation::Metric::Counter* unbuiltLayerRequests = nullptr;  // cellfield.unbuilt_layer_requests
            Dia::Observation::Metric::Counter* rejectedRequests = nullptr;      // cellfield.rejected_requests
            Dia::Observation::Metric::Counter* drawCalls = nullptr;             // cellfield.draw_calls
            // Gauges: set to the current value.
            Dia::Observation::Metric::Gauge*   queueDepth = nullptr;            // cellfield.queue_depth
            Dia::Observation::Metric::Gauge*   queueBytes = nullptr;            // cellfield.queue_bytes
            Dia::Observation::Metric::Gauge*   layersLive = nullptr;            // cellfield.layers_live
            Dia::Observation::Metric::Gauge*   geometryBytes = nullptr;         // cellfield.geometry_bytes
            Dia::Observation::Metric::Gauge*   texturesResident = nullptr;      // cellfield.textures_resident

            bool IsBound() const;
        };

        // Registers (idempotently) the sixteen cellfield.* metrics and returns their primitives.
        CellFieldMetrics RegisterCellFieldMetrics(Dia::Observation::Metric::MetricRegistry& registry);

        // Everything one Update needs; all cumulative stats come straight from the stats structs.
        struct CellFieldObservationInput
        {
            Dia::Graphics::CellFieldStats registry;
            CellFieldRendererStats        renderer;
            bool registrySet = false;   // a registry was given to the Canvas (so a renderer is expected)
            bool programValid = false;  // the CellField shader program loaded and a renderer exists
        };

        class CellFieldHealthReporter : public Dia::Observation::Health::HealthReporterBase
        {
        public:
            Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("DiaBgfx.CellField"); }
            Dia::Observation::Health::Health Report() const override { return HealthReporterBase::Report(); }
        };

        // Publishes CellField metrics and health. Render thread: call Update once per rendered frame.
        //
        // Health: Failing (reason cellfield.program_missing) when a registry is set but the CellField
        // shader program is not usable. Degraded when, since the previous Update, the registry dropped
        // commands at the byte cap (cellfield.dropped_commands), a draw/update hit a live layer whose
        // build failed (cellfield.unbuilt_layers) or a draw named an unregistered colour map
        // (cellfield.unknown_colour_maps). Back to OK once no condition has been seen for
        // kRecoveryFrames consecutive Updates (120 = two seconds at 60 fps).
        class CellFieldObservation
        {
        public:
            static constexpr uint32_t kRecoveryFrames = 120;

            void SetMetrics(const CellFieldMetrics& metrics);
            void Update(const CellFieldObservationInput& input);
            CellFieldHealthReporter& GetHealthReporter() { return mHealth; }

        private:
            static void Publish(Dia::Observation::Metric::Counter* c, uint64_t current, uint64_t& previous);

            CellFieldMetrics        mMetrics;
            CellFieldHealthReporter mHealth;
            // Previous cumulative values (counter deltas + health condition detection).
            uint64_t mPrevUploads = 0, mPrevUploadedBytes = 0, mPrevGeometryBuilds = 0, mPrevDropped = 0;
            uint64_t mPrevCoalesced = 0, mPrevStale = 0, mPrevUnknown = 0, mPrevOutOfRange = 0;
            uint64_t mPrevUnbuilt = 0, mPrevRejected = 0, mPrevDrawCalls = 0;
            uint32_t mClearFrames = 0;
            bool     mHealthy = true;
        };
    } // namespace Bgfx
} // namespace Dia
