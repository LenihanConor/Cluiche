#include <diaentitytemplate/JsonBlueprintLoader.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/ComponentRegistry.h>
#include <diaentitytemplate/ComponentTypeDesc.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::Entity {

    // Maximum components allowed per entity in a blueprint.
    static constexpr uint32_t kMaxComponentsPerEntity = 32;

    struct PendingComponent {
        Dia::Core::StringCRC typeId;
        const Json::Value*   config; // pointer into blueprint JSON (no copy)
        bool                 queued;
    };

    // ---------------------------------------------------------------------------
    // Load — entry point
    // ---------------------------------------------------------------------------

    bool JsonBlueprintLoader::Load(Domain& domain, const Json::Value& blueprint) {
        // Validate schema version.
        if (!blueprint.isMember("version") || !blueprint["version"].isInt()) {
            DIA_LOG_WARNING("diaentitytemplate", "JsonBlueprintLoader: missing or non-integer 'version' field");
            return false;
        }

        const int version = blueprint["version"].asInt();
        if (version != kBlueprintSchemaVersion) {
            DIA_LOG_WARNING("diaentitytemplate",
                "JsonBlueprintLoader: blueprint version %d does not match expected %d — aborting",
                version, kBlueprintSchemaVersion);
            return false;
        }

        // Pass 1 — instantiate entities and queue component adds.
        Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain> handles;

        if (blueprint.isMember("entities")) {
            if (!InstantiateEntities(domain, blueprint["entities"], handles)) {
                return false;
            }
        }

        // Pass 2 — patch EntityRef fields (v1: deferred until F4, log and skip).
        if (blueprint.isMember("references")) {
            if (!PatchReferences(domain, blueprint["references"], handles)) {
                return false;
            }
        }

        // Pass 3 — validate references (v1: always succeeds; no EntityRef fields yet).
        return ValidateReferences(domain, handles);
    }

    // ---------------------------------------------------------------------------
    // Pass 1 — InstantiateEntities
    // ---------------------------------------------------------------------------

    bool JsonBlueprintLoader::InstantiateEntities(
        Domain& domain,
        const Json::Value& entities,
        Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& outHandles)
    {
        if (!entities.isObject()) {
            DIA_LOG_WARNING("diaentitytemplate", "JsonBlueprintLoader: 'entities' is not an object");
            return false;
        }

        for (const auto& entityName : entities.getMemberNames()) {
            const Json::Value& entityObj = entities[entityName];

            if (!entityObj.isObject()) {
                DIA_LOG_WARNING("diaentitytemplate",
                    "JsonBlueprintLoader: entity '%s' value is not an object — skipping",
                    entityName.c_str());
                continue;
            }

            Entity entity = domain.CreateEntity(entityName.c_str());
            if (!entity.IsValid()) {
                DIA_LOG_WARNING("diaentitytemplate",
                    "JsonBlueprintLoader: entity pool full — could not create entity '%s'",
                    entityName.c_str());
                return false;
            }

            EntityNameHandle handle;
            handle.name   = Dia::Core::StringCRC(entityName.c_str());
            handle.entity = entity;

            if (!outHandles.IsFull()) {
                outHandles.Add(handle);
            }

            // Collect pending components for this entity.
            Dia::Core::Containers::DynamicArrayC<PendingComponent, kMaxComponentsPerEntity> pending;

            for (const auto& componentName : entityObj.getMemberNames()) {
                const Dia::Entity::ComponentTypeDesc* desc =
                    ComponentRegistry::Get().Find(Dia::Core::StringCRC(componentName.c_str()));

                if (desc == nullptr) {
                    DIA_LOG_WARNING("diaentitytemplate",
                        "JsonBlueprintLoader: unknown component type '%s' — skipping",
                        componentName.c_str());
                    continue;
                }

                if (!pending.IsFull()) {
                    PendingComponent pc;
                    pc.typeId  = desc->typeId;
                    pc.config  = &entityObj[componentName];
                    pc.queued  = false;
                    pending.Add(pc);
                }
            }

            // Topological sort: queue components in REQUIRES dependency order.
            // Simple iterative approach: loop until all queued or no progress.
            bool progress = true;
            while (progress) {
                progress = false;
                for (uint32_t i = 0; i < pending.Size(); ++i) {
                    if (pending[i].queued) continue;

                    const Dia::Entity::ComponentTypeDesc* desc =
                        ComponentRegistry::Get().Find(pending[i].typeId);

                    if (desc == nullptr) {
                        // Shouldn't happen since we already checked, but guard anyway.
                        pending[i].queued = true;
                        progress = true;
                        continue;
                    }

                    // Check that all required types are already queued.
                    bool allReqsMet = true;
                    for (uint16_t r = 0; r < desc->requiresCount; ++r) {
                        bool found = false;
                        for (uint32_t j = 0; j < pending.Size(); ++j) {
                            if (pending[j].typeId == desc->requires_[r] && pending[j].queued) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            allReqsMet = false;
                            break;
                        }
                    }

                    if (allReqsMet) {
                        domain.QueueAddComponentByTypeId(entity, pending[i].typeId, *pending[i].config);
                        pending[i].queued = true;
                        progress = true;
                    }
                }
            }

            // Warn about any unqueued components (unsatisfied dependencies).
            for (uint32_t i = 0; i < pending.Size(); ++i) {
                if (!pending[i].queued) {
                    DIA_LOG_WARNING("diaentitytemplate",
                        "JsonBlueprintLoader: component (CRC %u) on entity '%s' has unsatisfied REQUIRES — skipping",
                        pending[i].typeId.Value(), entityName.c_str());
                }
            }
        }

        return true;
    }

    // ---------------------------------------------------------------------------
    // Pass 2 — PatchReferences
    // ---------------------------------------------------------------------------

    bool JsonBlueprintLoader::PatchReferences(
        Domain& /*domain*/,
        const Json::Value& references,
        const Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& /*handles*/)
    {
        // EntityRef patching requires F4 (EntityRef<T> field type).
        // For v1 this is a no-op; log a warning if any references are present.
        if (references.isArray() && references.size() > 0) {
            DIA_LOG_WARNING("diaentitytemplate",
                "JsonBlueprintLoader: 'references' block has %u entries but EntityRef patching "
                "requires F4 — skipping reference wiring",
                references.size());
        }
        return true;
    }

    // ---------------------------------------------------------------------------
    // Pass 3 — ValidateReferences
    // ---------------------------------------------------------------------------

    bool JsonBlueprintLoader::ValidateReferences(
        Domain& /*domain*/,
        const Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& /*handles*/)
    {
        // No EntityRef fields exist in v1 — validation always succeeds.
        return true;
    }

} // namespace Dia::Entity
