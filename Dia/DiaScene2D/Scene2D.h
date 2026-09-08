////////////////////////////////////////////////////////////////////////////////
// Filename: Scene2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace Scene2D
    {
        ////////////////////////////////////////////////////////////
        /// \brief Layer definition — draw order, parallax, render policy.
        ////////////////////////////////////////////////////////////
        struct LayerDef
        {
            Dia::Core::StringCRC id;
            int                  sortOrder        = 0;
            Dia::Maths::Vector2D parallax         = {1.0f, 1.0f};
            Dia::Core::StringCRC sortPolicy;        // v1: "insertion" only
            bool                 enabled          = true;
            Dia::Core::StringCRC renderTechnique;   // optional, StringCRC::kEmpty if unset
        };

        ////////////////////////////////////////////////////////////
        /// \brief Camera entry in a scene file.
        ///
        /// blueprint + instanceData are resolved at load time by SceneLoader2D.
        /// instanceData is stored raw so the format layer has no diaentitytemplate dependency.
        ////////////////////////////////////////////////////////////
        struct CameraEntry
        {
            Dia::Core::StringCRC id;
            bool                 active    = false;
            Dia::Core::StringCRC blueprint;
            Json::Value          instanceData;   // opaque — "Component.Field": value map
        };

        ////////////////////////////////////////////////////////////
        /// \brief Light entry in a scene file.
        ////////////////////////////////////////////////////////////
        struct LightEntry
        {
            Dia::Core::StringCRC                                           id;
            bool                                                           enabled = true;
            Dia::Core::StringCRC                                           blueprint;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> affectsLayers;
            Json::Value                                                    instanceData;   // opaque
        };

        ////////////////////////////////////////////////////////////
        /// \brief Entity placement in a scene file.
        ////////////////////////////////////////////////////////////
        struct EntityInstance
        {
            Dia::Core::StringCRC id;
            Dia::Core::StringCRC name;      // optional (kEmpty if unset)
            Dia::Core::StringCRC blueprint;
            bool                 enabled = true;
            Json::Value          instanceData;   // opaque — "Component.Field": value map
        };

        ////////////////////////////////////////////////////////////
        /// \brief Root scene struct — serialized from a .diascene file.
        ///
        /// Purely spatial: no gameplay config, no identity, no rendering policy.
        ////////////////////////////////////////////////////////////
        struct Scene2D
        {
            Dia::Geometry2D::AARect                                           worldBounds;  // zero-area = unbounded
            Dia::Core::Containers::DynamicArrayC<LayerDef, 32>               layers;
            Dia::Core::Containers::DynamicArrayC<CameraEntry, 4>             cameras;
            Dia::Core::Containers::DynamicArrayC<LightEntry, 16>             lights;
            Dia::Core::Containers::DynamicArrayC<EntityInstance, 256>        entities;
        };

    } // namespace Scene2D
} // namespace Dia
