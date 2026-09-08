#pragma once

#include <DiaEntity/Entity.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::Entity { class IEntityInspectable; }

namespace Dia::EntityInspector {

    // Serializes the current state of a specific entity from a Domain into
    // the entity.inspect JSON payload. Called from the sim thread only —
    // IEntityInspectable is not thread-safe across thread boundaries.
    //
    // Returns a JSON object matching the entity.inspect schema:
    //   { entity, components, hierarchy, queries, mailbox_log }
    // Returns an empty object if the entity handle is not alive.
    Json::Value SerializeEntityInspect(
        const Dia::Entity::IEntityInspectable& domain,
        Dia::Entity::Entity entity);

    // Returns a lightweight entity summary (index, gen, debug_name, component_tag_count)
    // for all live entities. Used by entity.list queries.
    Json::Value SerializeEntityList(const Dia::Entity::IEntityInspectable& domain);

    // Produces the combined payload the editor UI expects on the entity.inspect topic:
    //   { frame, entityCount, entities: [{i,g,n,t,d,components:[{name,fields:[{n,v,t}]}]}] }
    // Includes all live entities with full component/field data.
    struct EntityDebugInfo {
        Dia::Entity::Entity entity;
        const char*         debugName;  // may be nullptr
    };
    Json::Value SerializeInspectPayload(
        const Dia::Entity::IEntityInspectable& inspectable,
        const EntityDebugInfo* entities,
        uint32_t entityCount,
        uint64_t frame);

} // namespace Dia::EntityInspector
