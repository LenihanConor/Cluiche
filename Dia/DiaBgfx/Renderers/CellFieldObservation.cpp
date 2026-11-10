////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldObservation.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/CellFieldObservation.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
    namespace Bgfx
    {
        namespace Metric = Dia::Observation::Metric;

        bool CellFieldMetrics::IsBound() const
        {
            return uploads && uploadedBytes && geometryBuilds && droppedCommands && coalescedCommands && staleHandles
                && unknownColourMaps && outOfRangeCells && unbuiltLayerRequests && rejectedRequests && drawCalls
                && queueDepth && queueBytes && layersLive && geometryBytes && texturesResident;
        }

        CellFieldMetrics RegisterCellFieldMetrics(Metric::MetricRegistry& reg)
        {
            using Dia::Core::StringCRC;
            CellFieldMetrics m;
            m.uploads              = reg.RegisterCounter(StringCRC("cellfield.uploads"));
            m.uploadedBytes        = reg.RegisterCounter(StringCRC("cellfield.uploaded_bytes"));
            m.geometryBuilds       = reg.RegisterCounter(StringCRC("cellfield.geometry_builds"));
            m.droppedCommands      = reg.RegisterCounter(StringCRC("cellfield.dropped_commands"));
            m.coalescedCommands    = reg.RegisterCounter(StringCRC("cellfield.coalesced_commands"));
            m.staleHandles         = reg.RegisterCounter(StringCRC("cellfield.stale_handles"));
            m.unknownColourMaps    = reg.RegisterCounter(StringCRC("cellfield.unknown_colour_maps"));
            m.outOfRangeCells      = reg.RegisterCounter(StringCRC("cellfield.out_of_range_cells"));
            m.unbuiltLayerRequests = reg.RegisterCounter(StringCRC("cellfield.unbuilt_layer_requests"));
            m.rejectedRequests     = reg.RegisterCounter(StringCRC("cellfield.rejected_requests"));
            m.drawCalls            = reg.RegisterCounter(StringCRC("cellfield.draw_calls"));
            m.queueDepth           = reg.RegisterGauge(StringCRC("cellfield.queue_depth"));
            m.queueBytes           = reg.RegisterGauge(StringCRC("cellfield.queue_bytes"));
            m.layersLive           = reg.RegisterGauge(StringCRC("cellfield.layers_live"));
            m.geometryBytes        = reg.RegisterGauge(StringCRC("cellfield.geometry_bytes"));
            m.texturesResident     = reg.RegisterGauge(StringCRC("cellfield.textures_resident"));
            return m;
        }

        void CellFieldObservation::SetMetrics(const CellFieldMetrics& metrics)
        {
            mMetrics = metrics;
            // Counters are process-lifetime; only deltas from now on are published.
            mPrevUploads = mPrevUploadedBytes = mPrevGeometryBuilds = mPrevDropped = 0;
            mPrevCoalesced = mPrevStale = mPrevUnknown = mPrevOutOfRange = 0;
            mPrevUnbuilt = mPrevRejected = mPrevDrawCalls = 0;
        }

        void CellFieldObservation::Publish(Metric::Counter* c, uint64_t current, uint64_t& previous)
        {
            // A smaller value means the source restarted (new registry/renderer): publish it whole.
            const uint64_t delta = current >= previous ? current - previous : current;
            previous = current;
            if (c && delta > 0)
                c->Inc(delta);
        }

        void CellFieldObservation::Update(const CellFieldObservationInput& in)
        {
            const Dia::Graphics::CellFieldStats& r = in.registry;
            const CellFieldRendererStats& s = in.renderer;

            // Health conditions are read from the deltas BEFORE Publish advances the baselines.
            const bool dropped = r.droppedCommands > mPrevDropped;
            const bool unbuilt = s.unbuiltLayerRequests > mPrevUnbuilt;
            const bool unknown = s.unknownColourMaps > mPrevUnknown;
            const bool programMissing = in.registrySet && !in.programValid;

            Publish(mMetrics.uploads, r.uploads, mPrevUploads);
            Publish(mMetrics.uploadedBytes, r.uploadedBytes, mPrevUploadedBytes);
            Publish(mMetrics.geometryBuilds, r.geometryBuilds, mPrevGeometryBuilds);
            Publish(mMetrics.droppedCommands, r.droppedCommands, mPrevDropped);
            Publish(mMetrics.coalescedCommands, r.coalescedCommands, mPrevCoalesced);
            Publish(mMetrics.staleHandles, r.staleHandles, mPrevStale);
            Publish(mMetrics.unknownColourMaps, s.unknownColourMaps, mPrevUnknown);
            Publish(mMetrics.outOfRangeCells, s.outOfRangeCategoryCells, mPrevOutOfRange);
            Publish(mMetrics.unbuiltLayerRequests, s.unbuiltLayerRequests, mPrevUnbuilt);
            Publish(mMetrics.rejectedRequests, r.rejectedRequests, mPrevRejected);
            Publish(mMetrics.drawCalls, s.drawCalls, mPrevDrawCalls);

            if (mMetrics.queueDepth)       mMetrics.queueDepth->Set(static_cast<double>(r.commandsQueued));
            if (mMetrics.queueBytes)       mMetrics.queueBytes->Set(static_cast<double>(r.queuedBytes));
            if (mMetrics.layersLive)       mMetrics.layersLive->Set(static_cast<double>(r.layersLive));
            if (mMetrics.geometryBytes)    mMetrics.geometryBytes->Set(static_cast<double>(s.geometryBytes));
            if (mMetrics.texturesResident) mMetrics.texturesResident->Set(static_cast<double>(s.colourMapsResident) + static_cast<double>(s.valueTexturesResident));

            // Health: highest-priority active condition wins.
            const char* failing = programMissing ? "cellfield.program_missing" : nullptr;
            const char* degraded = dropped ? "cellfield.dropped_commands"
                                 : unbuilt ? "cellfield.unbuilt_layers"
                                 : unknown ? "cellfield.unknown_colour_maps" : nullptr;
            if (failing)
            {
                if (mHealth.Report().status != Dia::Observation::Health::HealthStatus::kFailing)
                {
                    mHealth.IncrementErrors();
                    DIA_LOG_ERROR("CFL", "CellField health: Failing (%s) - the CellField shader program is not usable", failing);
                }
                mHealth.SetFailing(Dia::Core::StringCRC(failing));
                mHealthy = false;
                mClearFrames = 0;
            }
            else if (degraded)
            {
                const Dia::Observation::Health::Health prior = mHealth.Report();
                if (prior.status != Dia::Observation::Health::HealthStatus::kDegraded || !(prior.reason == Dia::Core::StringCRC(degraded)))
                {
                    mHealth.IncrementWarnings();
                    DIA_LOG_WARNING("CFL", "CellField health: Degraded (%s)", degraded);
                }
                mHealth.SetDegraded(Dia::Core::StringCRC(degraded));
                mHealthy = false;
                mClearFrames = 0;
            }
            else if (!mHealthy)
            {
                if (++mClearFrames >= kRecoveryFrames)
                {
                    mHealth.SetOK();
                    mHealthy = true;
                    mClearFrames = 0;
                    DIA_LOG_INFO("CFL", "CellField health: recovered to OK after %u clear frames", static_cast<unsigned>(kRecoveryFrames));
                }
            }
        }
    } // namespace Bgfx
} // namespace Dia
