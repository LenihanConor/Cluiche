#include "DiaEntityInspector/EntityInspectSerializer.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Log/DiaLog.h>

#include <string>
#include <cstdio>

namespace Dia::EntityInspector {

static Json::Value SerializeComponents(
    const Dia::Entity::IEntityInspectable& domain,
    Dia::Entity::Entity entity)
{
    using StringCRC = Dia::Core::StringCRC;
    using DynamicArrayC = Dia::Core::Containers::DynamicArrayC<StringCRC, 32>;

    DynamicArrayC typeIds;
    domain.GetComponentTypeIds(entity, typeIds);

    Json::Value components(Json::arrayValue);
    auto& registry = Dia::Entity::ComponentRegistry::Get();

    for (uint32_t ci = 0; ci < typeIds.Size(); ++ci)
    {
        const StringCRC typeId = typeIds[ci];
        const Dia::Entity::ComponentTypeDesc* desc = registry.Find(typeId);

        Json::Value comp(Json::objectValue);
        comp["type_id_crc"] = typeId.Value();
        comp["type_name"]   = desc ? desc->debugName : "unknown";

        Json::Value fields(Json::arrayValue);
        if (desc)
        {
            for (uint16_t fi = 0; fi < desc->fieldCount; ++fi)
            {
                const Dia::Entity::FieldDesc& fd = desc->fields[fi];
                Json::Value fieldJson(Json::objectValue);
                fieldJson["name"] = fd.name;
                fieldJson["kind"] = static_cast<int>(fd.kind);

                Json::Value value;
                if (domain.ReadField(entity, typeId, fd.name, value))
                    fieldJson["value"] = value;
                else
                {
                    DIA_LOG_WARNING("Editor", "SerializeComponents: ReadField failed for field '%s' on type '%s'",
                        fd.name, desc->debugName);
                    fieldJson["value"] = Json::Value(Json::nullValue);
                }

                fields.append(fieldJson);
            }
        }
        comp["fields"] = fields;
        components.append(comp);
    }
    return components;
}

static Json::Value SerializeQueries(
    const Dia::Entity::IEntityInspectable& domain,
    Dia::Entity::Entity entity)
{
    using StringCRC = Dia::Core::StringCRC;
    using SigArray = Dia::Core::Containers::DynamicArrayC<StringCRC, 32>;

    SigArray entityComponents;
    domain.GetComponentTypeIds(entity, entityComponents);

    auto& registry = Dia::Entity::ComponentRegistry::Get();
    const uint32_t queryCount = domain.GetQueryCount();

    Json::Value queries(Json::arrayValue);
    for (uint32_t qi = 0; qi < queryCount; ++qi)
    {
        SigArray signature;
        domain.GetQuerySignature(qi, signature);

        bool member = true;
        for (uint32_t si = 0; si < signature.Size(); ++si)
        {
            bool found = false;
            for (uint32_t ci = 0; ci < entityComponents.Size(); ++ci)
            {
                if (entityComponents[ci] == signature[si]) { found = true; break; }
            }
            if (!found) { member = false; break; }
        }

        Json::Value sigJson(Json::arrayValue);
        for (uint32_t si = 0; si < signature.Size(); ++si)
        {
            const StringCRC& crc = signature[si];
            const Dia::Entity::ComponentTypeDesc* desc = registry.Find(crc);
            Json::Value entry;
            entry["crc"]  = crc.Value();
            entry["name"] = desc ? desc->debugName : "";
            sigJson.append(entry);
        }

        Json::Value q;
        q["index"]        = static_cast<int>(qi);
        q["entity_count"] = static_cast<int>(domain.GetQueryEntityCount(qi));
        q["member"]       = member;
        q["signature"]    = sigJson;
        queries.append(q);
    }
    return queries;
}

Json::Value SerializeEntityInspect(
    const Dia::Entity::IEntityInspectable& domain,
    Dia::Entity::Entity entity)
{
    DIA_TRACE_ZONE("SerializeEntityInspect", Dia::Observation::Trace::Category::kNone);

    Json::Value result(Json::objectValue);

    if (!entity.IsValid())
        return result;

    // --- entity section ---
    Json::Value entityJson;
    entityJson["index"] = static_cast<int>(entity.GetIndex());
    entityJson["gen"]   = static_cast<int>(entity.GetGeneration());
    entityJson["debug_name"] = "";  // filled below in debug builds
    result["entity"] = entityJson;

    // --- components ---
    result["components"] = SerializeComponents(domain, entity);

    // --- hierarchy ---
    Json::Value hierarchy;
    hierarchy["parent_index"] = -1;
    hierarchy["parent_gen"]   = 0;
    hierarchy["child_count"]  = 0;
    hierarchy["children"]     = Json::Value(Json::arrayValue);

    static const Dia::Core::StringCRC kParentTypeId("dia.hierarchy.parent");
    static const Dia::Core::StringCRC kChildTypeId("dia.hierarchy.children");

    {
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> typeIds;
        domain.GetComponentTypeIds(entity, typeIds);
        bool hasParent = false;
        bool hasChildBuffer = false;
        for (uint32_t ci = 0; ci < typeIds.Size(); ++ci)
        {
            if (typeIds[ci] == kParentTypeId) hasParent = true;
            if (typeIds[ci] == kChildTypeId) hasChildBuffer = true;
        }

        if (hasParent)
        {
            Json::Value parentIdxVal;
            if (domain.ReadField(entity, kParentTypeId, "parentIndex", parentIdxVal))
            {
                hierarchy["parent_index"] = parentIdxVal.asInt();
                Json::Value parentGenVal;
                if (domain.ReadField(entity, kParentTypeId, "parentGen", parentGenVal))
                    hierarchy["parent_gen"] = parentGenVal.asInt();
            }
        }
        if (hasChildBuffer)
        {
            Json::Value childrenArr(Json::arrayValue);
            Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain> allEntities;
            domain.GetAllEntities(allEntities);
            for (uint32_t ei = 0; ei < allEntities.Size(); ++ei)
            {
                const Dia::Entity::Entity& candidate = allEntities[ei];
                Json::Value candParentIdx;
                if (domain.ReadField(candidate, kParentTypeId, "parentIndex", candParentIdx) &&
                    candParentIdx.asUInt() == entity.GetIndex())
                {
                    Json::Value candParentGen;
                    if (domain.ReadField(candidate, kParentTypeId, "parentGen", candParentGen) &&
                        candParentGen.asUInt() == entity.GetGeneration())
                    {
                        Json::Value child;
                        child["index"] = static_cast<int>(candidate.GetIndex());
                        child["gen"]   = static_cast<int>(candidate.GetGeneration());
                        childrenArr.append(child);
                    }
                }
            }
            hierarchy["child_count"] = static_cast<int>(childrenArr.size());
            hierarchy["children"]    = childrenArr;
        }
    }

    result["hierarchy"] = hierarchy;

    // --- queries ---
    result["queries"] = SerializeQueries(domain, entity);

    // --- mailbox_log (ring buffer maintained client-side in v1) ---
    result["mailbox_log"] = Json::Value(Json::arrayValue);

    return result;
}

Json::Value SerializeEntityList(const Dia::Entity::IEntityInspectable& domain)
{
    DIA_TRACE_ZONE("SerializeEntityList", Dia::Observation::Trace::Category::kNone);

    using Entity = Dia::Entity::Entity;
    using StringCRC = Dia::Core::StringCRC;
    Dia::Core::Containers::DynamicArrayC<Entity, Dia::Entity::kMaxEntitiesPerDomain> entities;
    domain.GetAllEntities(entities);

    Json::Value list(Json::arrayValue);
    for (uint32_t i = 0; i < entities.Size(); ++i)
    {
        const Entity& e = entities[i];
        Dia::Core::Containers::DynamicArrayC<StringCRC, 32> typeIds;
        domain.GetComponentTypeIds(e, typeIds);

        Json::Value entry;
        entry["index"]           = static_cast<int>(e.GetIndex());
        entry["gen"]             = static_cast<int>(e.GetGeneration());
        entry["component_count"] = static_cast<int>(typeIds.Size());
        list.append(entry);
    }
    return list;
}

static const char* FieldKindToTypeString(Dia::Entity::FieldKind kind)
{
    switch (kind)
    {
        case Dia::Entity::FieldKind::Primitive:   return "prim";
        case Dia::Entity::FieldKind::StringId:    return "str";
        case Dia::Entity::FieldKind::Math:        return "math";
        case Dia::Entity::FieldKind::AssetHandle: return "asset";
        case Dia::Entity::FieldKind::EntityRef:   return "eref";
        case Dia::Entity::FieldKind::Nested:      return "nest";
        case Dia::Entity::FieldKind::Container:   return "arr";
    }
    return "?";
}

static std::string ValueToString(const Json::Value& v)
{
    if (v.isNull())    return "null";
    if (v.isBool())    return v.asBool() ? "true" : "false";
    if (v.isInt())     return std::to_string(v.asInt());
    if (v.isUInt())    return std::to_string(v.asUInt());
    if (v.isDouble())  { char buf[32]; snprintf(buf, sizeof(buf), "%.4g", v.asDouble()); return buf; }
    if (v.isString())  return v.asString();
    if (v.isArray() || v.isObject())
    {
        Json::FastWriter writer;
        std::string s = writer.write(v);
        if (!s.empty() && s.back() == '\n') s.pop_back();
        return s;
    }
    return "?";
}

Json::Value SerializeInspectPayload(
    const Dia::Entity::IEntityInspectable& inspectable,
    const EntityDebugInfo* entities,
    uint32_t entityCount,
    uint64_t frame)
{
    DIA_TRACE_ZONE("SerializeInspectPayload", Dia::Observation::Trace::Category::kNone);

    using StringCRC = Dia::Core::StringCRC;
    auto& registry = Dia::Entity::ComponentRegistry::Get();

    Json::Value result(Json::objectValue);
    result["frame"]       = static_cast<Json::UInt64>(frame);
    result["entityCount"] = static_cast<int>(entityCount);

    Json::Value entitiesJson(Json::arrayValue);
    for (uint32_t ei = 0; ei < entityCount; ++ei)
    {
        const EntityDebugInfo& info = entities[ei];
        Dia::Core::Containers::DynamicArrayC<StringCRC, 32> typeIds;
        inspectable.GetComponentTypeIds(info.entity, typeIds);

        Json::Value entityJson(Json::objectValue);
        entityJson["i"] = static_cast<int>(info.entity.GetIndex());
        entityJson["g"] = static_cast<int>(info.entity.GetGeneration());
        entityJson["n"] = info.debugName ? info.debugName : "";

        static const Dia::Core::StringCRC kParentTypeId("dia.hierarchy.parent");
        int depth = 0;
        int parentIdx = -1;
        int parentGen = 0;
        {
            bool hasParent = false;
            for (uint32_t ti = 0; ti < typeIds.Size(); ++ti)
            {
                if (typeIds[ti] == kParentTypeId) { hasParent = true; break; }
            }
            if (hasParent)
            {
                Json::Value pIdx;
                if (inspectable.ReadField(info.entity, kParentTypeId, "parentIndex", pIdx))
                {
                    parentIdx = pIdx.asInt();
                    Json::Value pGen;
                    if (inspectable.ReadField(info.entity, kParentTypeId, "parentGen", pGen))
                        parentGen = pGen.asInt();
                    Dia::Entity::Entity walker(static_cast<uint32_t>(parentIdx), static_cast<uint32_t>(parentGen));
                    while (walker.IsValid() && depth < 16)
                    {
                        depth++;
                        Json::Value wIdx;
                        if (!inspectable.ReadField(walker, kParentTypeId, "parentIndex", wIdx))
                            break;
                        Json::Value wGen;
                        if (!inspectable.ReadField(walker, kParentTypeId, "parentGen", wGen))
                            break;
                        walker = Dia::Entity::Entity(wIdx.asUInt(), wGen.asUInt());
                    }
                }
            }
        }
        entityJson["d"]  = depth;
        entityJson["pi"] = parentIdx;
        entityJson["pg"] = parentGen;

        Json::Value tags(Json::arrayValue);
        Json::Value components(Json::arrayValue);
        for (uint32_t ci = 0; ci < typeIds.Size(); ++ci)
        {
            const StringCRC typeId = typeIds[ci];
            const Dia::Entity::ComponentTypeDesc* desc = registry.Find(typeId);
            const char* compName = desc ? desc->debugName : "Unknown";

            if (compName[0] != '\0')
                tags.append(Json::Value(std::string(1, compName[0])));

            Json::Value comp(Json::objectValue);
            comp["name"] = compName;

            Json::Value fields(Json::arrayValue);
            if (desc)
            {
                for (uint16_t fi = 0; fi < desc->fieldCount; ++fi)
                {
                    const Dia::Entity::FieldDesc& fd = desc->fields[fi];
                    Json::Value fieldJson(Json::objectValue);
                    fieldJson["n"] = fd.name;
                    fieldJson["t"] = FieldKindToTypeString(fd.kind);

                    Json::Value rawValue;
                    if (inspectable.ReadField(info.entity, typeId, fd.name, rawValue))
                        fieldJson["v"] = ValueToString(rawValue);
                    else
                        fieldJson["v"] = "?";

                    fields.append(fieldJson);
                }
            }
            comp["fields"] = fields;
            components.append(comp);
        }
        entityJson["t"] = tags;
        entityJson["components"] = components;
        entitiesJson.append(entityJson);
    }
    result["entities"] = entitiesJson;
    return result;
}

} // namespace Dia::EntityInspector
