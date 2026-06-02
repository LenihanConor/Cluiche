#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/MailboxTypes.h>
#include "Modules/EntityModule.h"
#include "Modules/VisualDebuggerModule.h"
#include "Modules/PickingModule.h"
#include <memory>

namespace Dia::EntityVisualDebugger
{
    class EntityLabelsDrawer;
    class EntityStatsDrawer;
    class HierarchyLinesDrawer;
    class ComponentFilterHighlightDrawer;
    class EntityPickingDrawer;
    class SelectionInspectorDrawer;
}

namespace Cluiche { namespace AppFlow {

class EntityVisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Entity debug overlays: labels, hierarchy, stats, picking, inspector";
    explicit EntityVisualDebuggerModule(const Dia::Core::StringCRC& instanceId);
    ~EntityVisualDebuggerModule() override;

    void SetPositionComponentTypeId(Dia::Core::StringCRC typeId) { mPositionTypeId = typeId; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void RegisterDrawers();
    void UnregisterDrawers();

    Dia::ApplicationFlow::ModuleRef<EntityModule>          mEntityRef{this};
    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule>  mVisualDebuggerRef{this};
    Dia::ApplicationFlow::ModuleRef<PickingModule>         mPickingRef{this};

    Dia::Core::StringCRC mPositionTypeId;

    std::unique_ptr<Dia::EntityVisualDebugger::EntityLabelsDrawer>            mLabelsDrawer;
    std::unique_ptr<Dia::EntityVisualDebugger::EntityStatsDrawer>             mStatsDrawer;
    std::unique_ptr<Dia::EntityVisualDebugger::HierarchyLinesDrawer>          mHierarchyDrawer;
    std::unique_ptr<Dia::EntityVisualDebugger::ComponentFilterHighlightDrawer> mHighlightDrawer;
    std::unique_ptr<Dia::EntityVisualDebugger::EntityPickingDrawer>           mPickingDrawer;
    std::unique_ptr<Dia::EntityVisualDebugger::SelectionInspectorDrawer>      mInspectorDrawer;

    Dia::Mailbox::SubscriberId mPickSubscriberId{};
    bool mPickingSubscribed = false;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
