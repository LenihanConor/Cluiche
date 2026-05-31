////////////////////////////////////////////////////////////////////////////////
// Filename: PickingModule.h
// Description: SimPU module that orchestrates 2D picking. Reads input from
//              InputStreamModule, transforms screen→world via CameraModule,
//              queries PickingService2D, and sends PickEvent<PickHit2D> via
//              DiaMailbox on each trigger (click, hover, right-click).
//
//              Consumers register their pickables via GetService(), subscribe
//              to triggers via GetRouter(), and drain events via GetMailbox().
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaPicking/PickEvent.h>
#include <DiaPicking/PickRouter.h>
#include <DiaGeometry2DPicking/PickingService2D.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include "Modules/InputStreamModule.h"
#include "Modules/CameraModule.h"

namespace Cluiche { namespace AppFlow {

class PickingModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "2D picking: screen→world queries sent via DiaMailbox";

    explicit PickingModule(const Dia::Core::StringCRC& instanceId);

    // Consumer API
    Dia::Geometry2DPicking::PickingService2D& GetService() { return mService; }
    Dia::Picking::PickRouter&                 GetRouter()  { return mRouter; }
    Dia::Mailbox::Mailbox&                    GetMailbox() { return mMailbox; }


protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()  override;

private:
    using PickEvent2D = Dia::Picking::PickEvent<Dia::Geometry2DPicking::PickHit2D>;

    Dia::Geometry2DPicking::PickingService2D mService;
    Dia::Mailbox::Mailbox                    mMailbox;
    Dia::Picking::PickRouter                 mRouter;

    Dia::ApplicationFlow::ModuleRef<InputStreamModule> mInputRef{this};
    Dia::ApplicationFlow::ModuleRef<CameraModule>      mCameraRef{this};
};

} } // namespace Cluiche::AppFlow
