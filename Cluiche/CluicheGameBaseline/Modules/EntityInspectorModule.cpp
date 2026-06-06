#include "Modules/EntityInspectorModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaEntityInspector/EntityInspectSerializer.h>
#include <DiaEntity/DebugDataTypes.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaEntity/Domain.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC EntityInspectorModule::kTypeId("EntityInspectorModule");

EntityInspectorModule::EntityInspectorModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

EntityInspectorModule::~EntityInspectorModule() = default;

Dia::ApplicationFlow::StartResult EntityInspectorModule::DoStart()
{
    DIA_LOG_INFO("Application", "EntityInspectorModule::DoStart");

    // Resolve the DebugServer pointer via the cross-PU service stream.
    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(GetApplication());
    if (app)
    {
        auto* store = app->FindStream(
            Dia::Core::StringCRC(DebugServerHostModule::kServiceStreamId));
        auto* typed = dynamic_cast<
            Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store);
        if (typed)
            mDebugServer = typed->Get();
        else
            DIA_LOG_WARNING("Application", "EntityInspectorModule: DebugServerService stream not found or wrong type");
    }

    if (mDebugServer)
        RegisterHandlers();
    else
        DIA_LOG_WARNING("Application", "EntityInspectorModule: no DebugServer — handlers not registered");

    return Dia::ApplicationFlow::StartResult::kReady;
}

void EntityInspectorModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("EntityInspectorModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    auto* vd = mVisualDebuggerRef.Get();
    if (!vd || !mDebugServer) return;

    const uint32_t selectedId = vd->GetLayerManager().GetSelectedEntityId();
    const bool selectionChanged = (selectedId != mLastSelectedId);

    ++mSlowPollCounter;
    const bool slowPollFire = (mSlowPollCounter >= kSlowPollInterval);

    if (selectionChanged || slowPollFire)
    {
        if (selectionChanged)
        {
            mLastSelectedId  = selectedId;
            mSlowPollCounter = 0;
        }
        else
        {
            mSlowPollCounter = 0;
        }

        PushInspect(selectedId);
    }
}

Dia::ApplicationFlow::StopResult EntityInspectorModule::DoStop()
{
    DIA_LOG_INFO("Application", "EntityInspectorModule::DoStop");
    UnregisterHandlers();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void EntityInspectorModule::PushInspect(uint32_t selectedId)
{
    DIA_TRACE_ZONE("EntityInspectorModule::PushInspect", Dia::Observation::Trace::Category::kNone);
    auto* entityMod = mEntityRef.Get();
    if (!entityMod || !mDebugServer) return;

    auto& inspectable = entityMod->GetInspectable();

    if (selectedId == 0)
    {
        // No entity selected — push empty payload so UI clears.
        DIA_LOG_INFO("Application", "EntityInspectorModule: no entity selected, clearing inspector UI");
        Json::Value empty;
        mDebugServer->NotifySubscribers(Dia::Entity::DebugDataType::kEntityInspect, empty);
        return;
    }

    // selectedId is stored as (entity.GetIndex() + 1) by the picking system.
    const uint32_t entityIndex = selectedId - 1;
    auto& domain = entityMod->GetDomain();
    Dia::Entity::Entity entity = domain.GetAliveEntity(entityIndex);

    if (!entity.IsValid())
    {
        DIA_LOG_WARNING("Application", "EntityInspectorModule: selectedId %u maps to dead entity", selectedId);
        return;
    }

    Json::Value payload = Dia::EntityInspector::SerializeEntityInspect(inspectable, entity);
    DIA_LOG_INFO("Application", "EntityInspectorModule: pushing entity.inspect for entityIndex=%u", entityIndex);
    mDebugServer->NotifySubscribers(Dia::Entity::DebugDataType::kEntityInspect, payload);
}

void EntityInspectorModule::RegisterHandlers()
{
    if (!mDebugServer) return;

    auto& queryReg = mDebugServer->GetQueryRegistry();

    // entity.inspect_request — immediate on-demand inspect of specific entity
    queryReg.Register(
        Dia::Entity::DebugDataType::kEntityInspectRequest,
        [this](const Json::Value& args) -> Json::Value
        {
            DIA_TRACE_ZONE("entity.inspect_request", Dia::Observation::Trace::Category::kNone);
            auto* entityMod = mEntityRef.Get();
            if (!entityMod) return Json::Value{};

            if (!args.isMember("index"))
                return Json::Value{};

            const uint32_t idx = static_cast<uint32_t>(args["index"].asInt());
            Dia::Entity::Entity entity = entityMod->GetDomain().GetAliveEntity(idx);
            if (!entity.IsValid()) return Json::Value{};

            return Dia::EntityInspector::SerializeEntityInspect(
                entityMod->GetInspectable(), entity);
        });

    // entity.find_by_name — O(N) scan, returns {index, gen} or {index: -1}
    (void)mDebugServer->GetCommandDispatcher();

    // Register DiaAPI JSON commands
    {
        Dia::API::CommandInfoJson writeField;
        writeField.name        = Dia::Entity::DebugDataType::kEntityWriteField;
        writeField.description = "Write a field value on a live entity component (editor only)";
        writeField.category    = Dia::Core::StringCRC("debug");
        writeField.owner       = "EntityInspectorModule";
        writeField.callback    = [this](const Json::Value& params) -> Json::Value
        {
            DIA_TRACE_ZONE("entity.write_field", Dia::Observation::Trace::Category::kNone);
            auto* entityMod = mEntityRef.Get();
            if (!entityMod) { Json::Value e; e["error"] = "entity module unavailable"; return e; }

            if (!params.isMember("index") || !params.isMember("componentCrc")
                || !params.isMember("field") || !params.isMember("value"))
            {
                Json::Value e; e["error"] = "missing index, componentCrc, field, or value";
                return e;
            }

            const uint32_t idx    = static_cast<uint32_t>(params["index"].asInt());
            const uint32_t crc    = static_cast<uint32_t>(params["componentCrc"].asUInt());
            const char*    field  = params["field"].asCString();
            const Json::Value& val = params["value"];

            Dia::Entity::Entity entity = entityMod->GetDomain().GetAliveEntity(idx);
            if (!entity.IsValid()) { Json::Value e; e["error"] = "entity not alive"; return e; }

            Dia::Core::StringCRC componentTypeId;
            static_cast<Dia::Core::CRC&>(componentTypeId) = crc;

            if (!entityMod->GetInspectable().WriteField(entity, componentTypeId, field, val))
            {
                Json::Value e; e["error"] = "WriteField failed";
                DIA_LOG_WARNING("Application", "entity.write_field: WriteField failed for field '%s'", field);
                return e;
            }

            Json::Value ok; ok["success"] = true;
            return ok;
        };
        Dia::API::RegisterCommandJson(writeField);
    }

    {
        Dia::API::CommandInfoJson findByName;
        findByName.name        = Dia::Entity::DebugDataType::kEntityFindByName;
        findByName.description = "Find a live entity by debug name (O(N) scan)";
        findByName.category    = Dia::Core::StringCRC("debug");
        findByName.owner       = "EntityInspectorModule";
        findByName.callback    = [this](const Json::Value& params) -> Json::Value
        {
            auto* entityMod = mEntityRef.Get();
            if (!entityMod) { Json::Value r; r["index"] = -1; return r; }

            if (!params.isMember("name")) { Json::Value r; r["index"] = -1; return r; }
            const char* searchName = params["name"].asCString();

            auto& domain = entityMod->GetDomain();
            Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain> entities;
            entityMod->GetInspectable().GetAllEntities(entities);

            for (uint32_t i = 0; i < entities.Size(); ++i)
            {
                const char* dbgName = domain.GetDebugName(entities[i]);
                if (dbgName && strcmp(dbgName, searchName) == 0)
                {
                    Json::Value r;
                    r["index"] = static_cast<int>(entities[i].GetIndex());
                    r["gen"]   = static_cast<int>(entities[i].GetGeneration());
                    return r;
                }
            }

            Json::Value r; r["index"] = -1;
            return r;
        };
        Dia::API::RegisterCommandJson(findByName);
    }
}

void EntityInspectorModule::UnregisterHandlers()
{
    DIA_LOG_INFO("Application", "EntityInspectorModule::UnregisterHandlers");
    if (!mDebugServer) return;
    mDebugServer->GetQueryRegistry().Unregister(Dia::Entity::DebugDataType::kEntityInspectRequest);
    mDebugServer = nullptr;
}

} } // namespace Cluiche::AppFlow

namespace { using EntityInspectorModule_ = Cluiche::AppFlow::EntityInspectorModule; }
DIA_MODULE(EntityInspectorModule_);
DIA_DESCRIBE(EntityInspectorModule_::kTypeId,
    "Pushes entity.inspect payloads to the debug server; registers entity.write_field and entity.find_by_name commands");

#endif // DIA_DEBUG
