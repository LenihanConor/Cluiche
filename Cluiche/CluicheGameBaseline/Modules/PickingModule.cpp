////////////////////////////////////////////////////////////////////////////////
// Filename: PickingModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Modules/PickingModule.h"

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaPicking/PickAddress.h>
#include <DiaPicking/PickTrigger.h>
#include <DiaGraphics/Camera/ViewportTransform.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC PickingModule::kTypeId("PickingModule");

PickingModule::PickingModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult PickingModule::DoStart()
{
    mMailbox.RegisterType<PickEvent2D, 16>();
    mMailbox.RegisterRouter(&mRouter);
    DIA_LOG_INFO("picking", "PickingModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void PickingModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    if (!mInputRef.Get() || !mCameraRef.Get()) return;

    const Dia::Graphics::ViewportTransform vt = mCameraRef->GetViewportTransform();
    const Dia::Maths::Vector2D screenPos(
        static_cast<float>(mInputRef->GetMouseX()),
        static_cast<float>(mInputRef->GetMouseY()));
    const Dia::Maths::Vector2D worldPos = vt.ScreenToWorld(screenPos);

    // Hover — every frame
    {
        PickEvent2D evt;
        evt.trigger = Dia::Picking::PickTrigger::kHover;
        evt.hits    = mService.Pick(worldPos);
        mMailbox.Send(Dia::Picking::PickAddress::ForTrigger(Dia::Picking::PickTrigger::kHover), evt);
    }

    // Left click
    if (mInputRef->WasMouseButtonPressed(0))
    {
        PickEvent2D evt;
        evt.trigger = Dia::Picking::PickTrigger::kClick;
        evt.hits    = mService.Pick(worldPos);
        mMailbox.Send(Dia::Picking::PickAddress::ForTrigger(Dia::Picking::PickTrigger::kClick), evt);

        if (evt.hits.HasHit())
        {
            DIA_LOG_INFO("picking",
                "click: screen(%.0f,%.0f) world(%.2f,%.2f) hits=%u best='%s'",
                screenPos.x, screenPos.y,
                worldPos.x, worldPos.y,
                evt.hits.Count(),
                evt.hits.Best().pickableId.AsChar());
        }
        else
        {
            DIA_LOG_INFO("picking",
                "click: screen(%.0f,%.0f) world(%.2f,%.2f) — no hits",
                screenPos.x, screenPos.y, worldPos.x, worldPos.y);
        }
    }

    // Right click
    if (mInputRef->WasMouseButtonPressed(1))
    {
        PickEvent2D evt;
        evt.trigger = Dia::Picking::PickTrigger::kRightClick;
        evt.hits    = mService.Pick(worldPos);
        mMailbox.Send(Dia::Picking::PickAddress::ForTrigger(Dia::Picking::PickTrigger::kRightClick), evt);

        DIA_LOG_INFO("picking",
            "right-click: screen(%.0f,%.0f) world(%.2f,%.2f) hits=%u",
            screenPos.x, screenPos.y, worldPos.x, worldPos.y, evt.hits.Count());
    }
}

Dia::ApplicationFlow::StopResult PickingModule::DoStop()
{
    mService.Clear();
    DIA_LOG_INFO("picking", "PickingModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using PickingModule_ = Cluiche::AppFlow::PickingModule; }
DIA_MODULE(PickingModule_);
DIA_DESCRIBE(PickingModule_::kTypeId, "Handles mouse/cursor picking against scene geometry and publishes selection results.");
