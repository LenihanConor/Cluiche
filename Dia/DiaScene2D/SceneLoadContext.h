////////////////////////////////////////////////////////////////////////////////
// Filename: SceneLoadContext.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <diaentitytemplate/Domain.h>

namespace Dia
{
    namespace Scene2D
    {
        ////////////////////////////////////////////////////////////
        /// \brief Target systems for SceneLoader2D to populate.
        ///
        /// All references must remain valid for the lifetime of the loader.
        ////////////////////////////////////////////////////////////
        struct SceneLoadContext
        {
            Dia::Camera2D::CameraRegistry2D&     cameraRegistry;
            Dia::Lighting2D::LightRegistry2D&    lightRegistry;
            Dia::Entity::Domain&                 entityDomain;
        };

    } // namespace Scene2D
} // namespace Dia
