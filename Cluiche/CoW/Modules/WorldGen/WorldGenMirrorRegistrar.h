// Forwards DiaCellLayers' sim-hosted cells.* commands/queries to the SimQueryHostModule so their handlers run on this PU (SimPU).
// Dia::CellLayers cannot see Cluiche, so the application supplies this adapter. Same shape as CluicheTest's MirrorRegistrar
// (CellLayersViewerTestStageModule.h); CoW cannot include CluicheTest. Header-only (no translation unit to register).
#pragma once

#ifdef DIA_DEBUG

#include "Modules/SimQueryHostModule.h"
#include <DiaCellLayers/MirrorQueries.h>

#include <utility>

namespace CoW {

class WorldGenMirrorRegistrar : public Dia::CellLayers::IMirrorQueryRegistrar
{
public:
    explicit WorldGenMirrorRegistrar(Cluiche::AppFlow::SimQueryHostModule& host) : mHost(host) {}
    bool Register(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::CellLayers::MirrorQueryHandler handler) override
    {
        return mHost.RegisterQuery(name, help, std::move(handler));
    }
    bool RegisterCommand(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::CellLayers::MirrorQueryHandler handler) override
    {
        return mHost.RegisterCommand(name, help, std::move(handler));
    }
    void Unregister(const char* name) override { mHost.UnregisterQuery(name); }
private:
    Cluiche::AppFlow::SimQueryHostModule& mHost;
};

} // namespace CoW

#endif // DIA_DEBUG
