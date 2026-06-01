////////////////////////////////////////////////////////////////////////////////
// Filename: DiaScene2DSerializers.h
//
// serialize() free functions for DiaScene2D types.
//
// USAGE:
//   #include "DiaScene2D/DiaScene2DSerializers.h"
//
// NOTE: CameraEntry, LightEntry, EntityInstance store instanceData as raw
// Json::Value — it is captured verbatim during deserialization and resolved
// at load time by SceneLoader2D. No DiaEntity dependency at this layer.
//
// ADL NOTE:
//   All serialize() functions are in namespace Dia::Scene2D, matching
//   the types so ADL finds them automatically.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaGeometry2D/DiaGeometry2DSerializers.h"
#include "DiaCore/Reflect/DiaCoreSerializers.h"
#include "DiaScene2D/Scene2D.h"

namespace Dia::Scene2D {

// -----------------------------------------------------------------------------
// LayerDef
// -----------------------------------------------------------------------------
DIA_SERIALIZE(LayerDef, 1)
    DIA_FIELD_REQUIRED(id)
    DIA_FIELD(sortOrder)
    DIA_FIELD(parallax)
    DIA_FIELD(sortPolicy)
    DIA_FIELD(enabled)
    DIA_FIELD(renderTechnique)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// CameraEntry
// instanceData is stored as raw Json::Value — captured verbatim, not recursed.
// -----------------------------------------------------------------------------
template<class Archive>
void serialize(Archive& ar, CameraEntry& obj, unsigned /*version*/)
{
    static_assert(Dia::Reflect::Archive<Archive>,
                  "CameraEntry: Archive type does not satisfy Dia::Reflect::Archive concept");
    ar & Dia::Reflect::named("id",        obj.id);
    ar & Dia::Reflect::named("active",    obj.active);
    ar & Dia::Reflect::named("blueprint", obj.blueprint);
    if constexpr (std::is_same_v<Archive, Dia::Reflect::JsonReadArchive>) {
        // Capture instance_data as opaque Json::Value
        const Json::Value& node = ar.CurrentNodePublic();
        if (node.isMember("instance_data")) {
            obj.instanceData = node["instance_data"];
        }
    }
}

// -----------------------------------------------------------------------------
// LightEntry
// affectsLayers serialized as array of StringCRC strings; instanceData opaque.
// -----------------------------------------------------------------------------
template<class Archive>
void serialize(Archive& ar, LightEntry& obj, unsigned /*version*/)
{
    static_assert(Dia::Reflect::Archive<Archive>,
                  "LightEntry: Archive type does not satisfy Dia::Reflect::Archive concept");
    ar & Dia::Reflect::named("id",             obj.id);
    ar & Dia::Reflect::named("enabled",        obj.enabled);
    ar & Dia::Reflect::named("blueprint",      obj.blueprint);
    ar & Dia::Reflect::named("affects_layers", obj.affectsLayers);
    if constexpr (std::is_same_v<Archive, Dia::Reflect::JsonReadArchive>) {
        const Json::Value& node = ar.CurrentNodePublic();
        if (node.isMember("instance_data")) {
            obj.instanceData = node["instance_data"];
        }
    }
}

// -----------------------------------------------------------------------------
// EntityInstance
// instanceData opaque.
// -----------------------------------------------------------------------------
template<class Archive>
void serialize(Archive& ar, EntityInstance& obj, unsigned /*version*/)
{
    static_assert(Dia::Reflect::Archive<Archive>,
                  "EntityInstance: Archive type does not satisfy Dia::Reflect::Archive concept");
    ar & Dia::Reflect::named("id",        obj.id);
    ar & Dia::Reflect::named("name",      obj.name);
    ar & Dia::Reflect::named("blueprint", obj.blueprint).Required();
    ar & Dia::Reflect::named("enabled",   obj.enabled);
    if constexpr (std::is_same_v<Archive, Dia::Reflect::JsonReadArchive>) {
        const Json::Value& node = ar.CurrentNodePublic();
        if (node.isMember("instance_data")) {
            obj.instanceData = node["instance_data"];
        }
    }
}

// -----------------------------------------------------------------------------
// Scene2D
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Scene2D, 1)
    DIA_FIELD(worldBounds)
    DIA_FIELD(layers)
    DIA_FIELD(cameras)
    DIA_FIELD(lights)
    DIA_FIELD(entities)
DIA_SERIALIZE_END

} // namespace Dia::Scene2D
