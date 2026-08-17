////////////////////////////////////////////////////////////////////////////////
// Filename: AssetRuntimeDebugDomain.h
// Description: IDebugDomain wrapper for DiaAssetRuntimeVisualDebugger.
//              Bridges the monolithic asset-runtime drawer into the
//              IDebugDomain lifecycle and DiaDebugPanel state protocol.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug        { class DebugLayerManager; }
namespace Dia::AssetRuntime { class DiaAssetRuntimeVisualDebugger; }
namespace Dia::AssetRuntime { class AssetRuntime; }

namespace Dia::AssetRuntime
{

class AssetRuntimeDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 1;

    AssetRuntimeDebugDomain();
    ~AssetRuntimeDebugDomain() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return true; }

    // ---- IDebugDomain: lifecycle ----
    void Register(Dia::Debug::DebugLayerManager& mgr)   override;
    void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

    // Inject the runtime so GetJSONState() can populate live stats.
    // Must be called before GetJSONState() is first invoked (kMain thread).
    void SetRuntime(const Dia::AssetRuntime::AssetRuntime* runtime);

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- IDebugDomain: drawer access ----
    int                          GetDrawerCount() const override;
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

private:
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    Dia::Debug::DebugLayerManager*                        mLayerManager = nullptr;
    std::unique_ptr<DiaAssetRuntimeVisualDebugger>        mDebugger;
    const Dia::AssetRuntime::AssetRuntime*                mRuntime      = nullptr;
};

} // namespace Dia::AssetRuntime

#endif // DIA_DEBUG
