#pragma once

#include <diaentitytemplate/Entity.h>
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

} // namespace Dia::EntityInspector
