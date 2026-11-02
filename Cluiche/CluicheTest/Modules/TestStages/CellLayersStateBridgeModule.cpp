#include "Modules/TestStages/CellLayersStateBridgeModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC CellLayersStateBridgeModule::kTypeId("CellLayersStateBridgeModule");

bool CellViewerStateChannel::ReadLatest(std::string& json)
{
    const Dia::CellLayers::ViewerStateSlot* slot = mReader != nullptr ? mReader->FetchLatest() : nullptr;
    if (slot == nullptr)
        return false;
    const uint32_t length = slot->length <= Dia::CellLayers::ViewerStateSlot::kCapacity ? slot->length : Dia::CellLayers::ViewerStateSlot::kCapacity;
    json.assign(slot->json, length);
    return true;
}

CellLayersStateBridgeModule::CellLayersStateBridgeModule(const Dia::Core::StringCRC& instanceId)
    : MainModule(instanceId)
{
    mChannel.SetSnapshotReader(&mSnapshotIn);
}

void CellLayersStateBridgeModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mSnapshotIn.Connect(app);
    mDemandOut.Connect(app);
    mChannelOut.Connect(app);
}

Dia::ApplicationFlow::StartResult CellLayersStateBridgeModule::DoStart()
{
    mChannelOut.Register(mChannel);
    DIA_LOG_INFO("CEL", "CellLayersStateBridgeModule: viewer state channel published on '%s'", kChannelStreamId);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void CellLayersStateBridgeModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    // One demand event per Main tick, however many GetJSONState calls the panel and queries made.
    if (mChannel.TakeDemand())
        mDemandOut.Send(Dia::CellLayers::ViewerStateDemandEvent{});
}

Dia::ApplicationFlow::StopResult CellLayersStateBridgeModule::DoStop()
{
    mChannelOut.Deregister();
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

namespace { using CellLayersStateBridgeModule_ = CluicheTest::CellLayersStateBridgeModule; }
DIA_MODULE(CellLayersStateBridgeModule_);
DIA_DESCRIBE(CellLayersStateBridgeModule_::kTypeId, "Main end of the cell-layer viewer state snapshot: reads the snapshot FrameStream, signals demand.");

#endif // DIA_DEBUG
