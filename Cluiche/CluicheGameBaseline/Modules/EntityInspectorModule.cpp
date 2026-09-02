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

#include "Modules/DebugServerHostModule.h"

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC EntityInspectorModule::kTypeId("EntityInspectorModule");

EntityInspectorModule::EntityInspectorModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

EntityInspectorModule::~EntityInspectorModule() = default;

void EntityInspectorModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mInspectWriter.Connect(app);
}

Dia::ApplicationFlow::StartResult EntityInspectorModule::DoStart()
{
    DIA_LOG_INFO("Application", "EntityInspectorModule::DoStart");

    // Resolve the DebugServer pointer via the cross-PU service stream.
    // Used only for one-time query/command handler registration (single-threaded,
    // safe — DoStart runs sequentially within this PU and MainPU has already started).
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

void EntityInspectorModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    DIA_TRACE_ZONE("EntityInspectorModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    ++mSlowPollCounter;
    if (mSlowPollCounter >= kSlowPollInterval)
    {
        mSlowPollCounter = 0;
        PushInspect(0);
    }
}

Dia::ApplicationFlow::StopResult EntityInspectorModule::DoStop()
{
    DIA_LOG_INFO("Application", "EntityInspectorModule::DoStop");
    UnregisterHandlers();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void EntityInspectorModule::PushInspect(uint32_t /*selectedId*/)
{
    DIA_TRACE_ZONE("EntityInspectorModule::PushInspect", Dia::Observation::Trace::Category::kNone);
    auto* entityMod = mEntityRef.Get();
    if (!entityMod)
    {
        DIA_LOG_WARNING("Application", "EntityInspectorModule::PushInspect — mEntityRef is null, skipping");
        return;
    }

    auto& inspectable = entityMod->GetInspectable();
    auto& domain = entityMod->GetDomain();

    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain> allEntities;
    inspectable.GetAllEntities(allEntities);

    if (allEntities.Size() == 0)
    {
        DIA_LOG_WARNING("Application", "EntityInspectorModule::PushInspect — 0 entities alive, skipping");
        return;
    }

    Dia::Core::Containers::DynamicArrayC<Dia::EntityInspector::EntityDebugInfo, Dia::Entity::kMaxEntitiesPerDomain> infos;
    for (uint32_t i = 0; i < allEntities.Size(); ++i)
    {
        Dia::EntityInspector::EntityDebugInfo info;
        info.entity    = allEntities[i];
        info.debugName = domain.GetDebugName(allEntities[i]);
        infos.Add(info);
    }

    ++mFrameCounter;
    DebugServerPushEvent evt;
    evt.dataType = Dia::Entity::DebugDataType::kEntityInspect;
    evt.payload  = Dia::EntityInspector::SerializeInspectPayload(
        inspectable, &infos[0], infos.Size(), mFrameCounter);

    DIA_LOG_INFO("Application", "EntityInspectorModule::PushInspect — frame=%llu entities=%u payloadNull=%d",
        mFrameCounter, infos.Size(), evt.payload.isNull() ? 1 : 0);
    mInspectWriter.Send(evt);
}

void EntityInspectorModule::RegisterHandlers()
{
    if (!mDebugServer) return;

    auto& queryReg = mDebugServer->GetQueryRegistry();

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

    (void)mDebugServer->GetCommandDispatcher();

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
