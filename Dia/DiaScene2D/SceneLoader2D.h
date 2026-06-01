////////////////////////////////////////////////////////////////////////////////
// Filename: SceneLoader2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEntity/Entity.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaScene2D/SceneLoadContext.h>

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
            // v1: binary flag; detailed per-entry errors deferred
        };

        ////////////////////////////////////////////////////////////
        /// \brief Loads a .diascene file and populates target systems.
        ///
        /// One loader per scene slot — tracks what it registered so
        /// Unload() can cleanly reverse it.
        ///
        /// Load() is not re-entrant. Call Unload() before re-loading.
        ////////////////////////////////////////////////////////////
        class SceneLoader2D
        {
        public:
            SceneLoader2D();

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
        };

    } // namespace Scene2D
} // namespace Dia
