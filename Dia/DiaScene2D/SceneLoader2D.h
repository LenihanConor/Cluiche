////////////////////////////////////////////////////////////////////////////////
// Filename: SceneLoader2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <diaentitytemplate/Entity.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaScene2D/SceneLoadContext.h>

namespace Dia { namespace Observation { namespace Metric { class Counter; class Gauge; } } }

namespace Dia
{
    namespace Scene2D
    {
        ////////////////////////////////////////////////////////////
        /// \brief Simple error accumulator for scene load failures.
        ////////////////////////////////////////////////////////////
        struct SceneLoadErrors
        {
            bool hasErrors = false;
        };

        ////////////////////////////////////////////////////////////
        /// \brief Loads a .diascene file and populates target systems.
        ///
        /// Inherits HealthReporterBase — reports kOK after a successful load,
        /// kFailing on hard errors (parse, missing key, camera validation).
        ///
        /// One loader per scene slot. Call Unload() before re-loading.
        ////////////////////////////////////////////////////////////
        class SceneLoader2D : public Dia::Observation::Health::HealthReporterBase
        {
        public:
            SceneLoader2D();

            Dia::Core::StringCRC GetReporterName() const override;

            // Load .diascene file — populates camera registry, light registry, and entity domain.
            // outLayers receives the built LayerTable.
            // Returns false on hard errors (parse failure, wrong top-level key, != 1 active camera).
            bool Load(const char*         filePath,
                      SceneLoadContext&   context,
                      LayerTable&         outLayers,
                      SceneLoadErrors*    outErrors = nullptr);

            // Unload — unregisters cameras/lights, destroys spawned entities.
            void Unload(SceneLoadContext& context);

        private:
            void ApplyInstanceData(Dia::Entity::Domain&        domain,
                                   Dia::Entity::Entity         entity,
                                   const Json::Value&          instanceData,
                                   SceneLoadErrors*            outErrors);

            // Tracking arrays for Unload
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>    mRegisteredCameras;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>   mRegisteredLights;
            Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 256>   mSpawnedEntities;

            // Metrics (owned by MetricRegistry)
            Dia::Observation::Metric::Counter* mMetricLoadsTotal     = nullptr;
            Dia::Observation::Metric::Counter* mMetricLoadFailures   = nullptr;
            Dia::Observation::Metric::Gauge*   mMetricCamerasLoaded  = nullptr;
            Dia::Observation::Metric::Gauge*   mMetricLightsLoaded   = nullptr;
            Dia::Observation::Metric::Gauge*   mMetricEntitiesLoaded = nullptr;
        };

    } // namespace Scene2D
} // namespace Dia
