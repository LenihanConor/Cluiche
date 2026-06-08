#include "Modules/InspectorSources/ModuleStateSource.h"
#include <DiaDebugServer/IDebugStateProvider.h>

namespace {
    const char* ToLifecycleState(Dia::ApplicationFlow::ModuleState s)
    {
        using Dia::ApplicationFlow::ModuleState;
        switch (s)
        {
            case ModuleState::kActive:   return "Running";
            case ModuleState::kStarting: return "Loading";
            case ModuleState::kStopping: return "Stopped";
            case ModuleState::kInactive: return "Stopped";
            case ModuleState::kFailed:   return "Failed";
        }
        return "Stopped";
    }
}

namespace Cluiche { namespace AppFlow {

ModuleStateSource::ModuleStateSource(Dia::ApplicationFlow::IApplicationInspectable* app)
    : mApp(app)
{
}

Dia::Core::StringCRC ModuleStateSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("app.modules");
    return kTopic;
}

Dia::DebugServer::SourcePolicy ModuleStateSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kChangeDetected, 0.0f, 0, 0.0f };
}

unsigned int ModuleStateSource::CollectAndHash(Json::Value& payload)
{
    if (!mApp) return 0;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
    mApp->GetProcessingUnits(puIds);

    unsigned int hash = 0;
    unsigned int moduleCount = 0;
    Json::Value modulesArray(Json::arrayValue);

    for (unsigned int p = 0; p < puIds.Size(); ++p)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::ModuleStateInfo, 64> modules;
        mApp->GetActiveModules(puIds[p], modules);

        for (unsigned int m = 0; m < modules.Size(); ++m)
        {
            const char* state = ToLifecycleState(modules[m].state);
            hash = HashCombine(hash, static_cast<unsigned int>(modules[m].instanceId.Value()));
            hash = HashCombine(hash, static_cast<unsigned int>(state[0]));
            ++moduleCount;

            Json::Value entry;
            entry["moduleId"]       = modules[m].instanceId.AsChar();
            entry["puId"]           = puIds[p].AsChar();
            entry["lifecycleState"] = state;
            entry["timeInStateMs"]  = 0;
            modulesArray.append(entry);
        }
    }
    hash = HashCombine(hash, moduleCount);

    payload["modules"] = modulesArray;
    return hash;
}

}} // namespace Cluiche::AppFlow
