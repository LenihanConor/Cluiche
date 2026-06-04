#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaEntity/Entity.h>

namespace Dia::Entity {

    // Read-only inspection and live field editing of entity state at runtime.
    // All field access is driven by ComponentTypeDesc reflection metadata.
    //
    // Tier (a): read-only — GetEntityCount, GetAllEntities, GetComponentTypeIds, ReadField
    // Tier (b): live field edit — WriteField (bypasses mutation queue, for editor use between frames only)
    // Tier (c): structural edit (create/destroy entity, add/remove component) — deferred (SD-ENT-016)
    class IEntityInspectable {
    public:
        virtual ~IEntityInspectable() = default;

        // Tier (a) — Read-only

        // Returns the number of currently live entities.
        virtual uint32_t GetEntityCount() const = 0;

        // Fills out with all live entity handles. Clears out before filling.
        virtual void GetAllEntities(
            Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain>& out) const = 0;

        // Fills out with all component type IDs attached to the given entity.
        // Clears out before filling. Returns immediately if entity is not alive.
        virtual void GetComponentTypeIds(
            Entity entity,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const = 0;

        // Reads a single field from a component as a Json::Value.
        // Returns false if: entity not alive, component not present, field name not found,
        // or no reflection descriptor registered for the component type.
        // On false return, DIA_LOG_WARNING is emitted.
        virtual bool ReadField(
            Entity entity,
            Dia::Core::StringCRC componentTypeId,
            const char* fieldName,
            Json::Value& out) const = 0;

        // Tier (b) — Live field edit
        // Bypasses mutation queue — for editor use between frames only.
        // Validates type compatibility via FieldDesc::kind before writing.
        // Returns false + DIA_LOG_WARNING on: entity/component/field not found, or type mismatch.
        // Never asserts on type mismatch.
        virtual bool WriteField(
            Entity entity,
            Dia::Core::StringCRC componentTypeId,
            const char* fieldName,
            const Json::Value& value) = 0;

        // Query cache introspection — used by EntityInspectSerializer to populate
        // the queries array in the entity.inspect payload.
        virtual uint32_t GetQueryCount() const = 0;

        // Fills out with the component type CRCs that form the signature of query at queryIndex.
        // Clears out before filling. No-op if queryIndex >= GetQueryCount().
        virtual void GetQuerySignature(
            uint32_t queryIndex,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const = 0;

        // Returns the number of entities currently matching query at queryIndex.
        // Returns 0 if queryIndex >= GetQueryCount().
        virtual uint32_t GetQueryEntityCount(uint32_t queryIndex) const = 0;

        // Mailbox access — provides access to the domain's message bus.
        virtual const Dia::Mailbox::Mailbox& GetMailbox() const = 0;
    };

} // namespace Dia::Entity
