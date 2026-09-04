#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaAttributeVisualDebugger/AttributeVisualDebugger.h>
#include "Modules/EntityModule.h"
#include "Modules/VisualDebuggerModule.h"
#include <memory>

namespace Cluiche { namespace AppFlow {

// Registers the panel-only DiaAttribute debug domain against the visual debugger.
// Unlike EntityVisualDebuggerModule this module has no picking involvement and no
// per-frame work — the domain pulls the current selection out of the layer manager
// (its IDebugContext) whenever the panel asks for state.
class AttributeVisualDebuggerModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Attribute inspector: values, ranges, modifier stack for the selection";

    explicit AttributeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId);
    ~AttributeVisualDebuggerModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;

    // Nothing to tick: the domain is entirely pull-driven from GetJSONState, which the
    // panel calls, plus push notifications from the observed AttributeSet.
    void DoUpdate(const Dia::SimTime::SimTimeContext&) override {}

    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    void RegisterDebugDomain();
    void UnregisterDebugDomain();

    Dia::ApplicationFlow::ModuleRef<EntityModule>         mEntityRef{this};
    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule> mVisualDebuggerRef{this};

    std::unique_ptr<Dia::AttributeVisualDebugger::AttributeVisualDebugger> mDebugDomain;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
