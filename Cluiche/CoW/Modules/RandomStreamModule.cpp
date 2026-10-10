#include "RandomStreamModule.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC RandomStreamModule::kTypeId("RandomStreamModule");

//-----------------------------------------------------------------------------
RandomStreamModule::RandomStreamModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

//-----------------------------------------------------------------------------
Dia::ApplicationFlow::StartResult RandomStreamModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;

    // Create the 8 streams with distinct seeds
    // Seeds chosen to be easily identifiable and well-separated
    // TODO: F-2 Core Runtime may want configurable seeds from .diaapp config
    mManager.CreateStream(Dia::Core::StringCRC("WLD"), 10000);  // World generation
    mManager.CreateStream(Dia::Core::StringCRC("A1"),  20000);
    mManager.CreateStream(Dia::Core::StringCRC("B4"),  30000);
    mManager.CreateStream(Dia::Core::StringCRC("B6"),  40000);
    mManager.CreateStream(Dia::Core::StringCRC("C1"),  50000);
    mManager.CreateStream(Dia::Core::StringCRC("C2"),  60000);
    mManager.CreateStream(Dia::Core::StringCRC("B5"),  70000);
    mManager.CreateStream(Dia::Core::StringCRC("C4"),  80000);

    DIA_LOG_INFO("CoW", "RandomStreamModule: Created 8 independent RNG streams for deterministic simulation");

    return StartResult::kReady;
}

//-----------------------------------------------------------------------------
Dia::ApplicationFlow::StopResult RandomStreamModule::DoStop()
{
    using Dia::ApplicationFlow::StopResult;

    // Manager destructor will clean up all streams
    DIA_LOG_INFO("CoW", "RandomStreamModule: Shutting down RNG streams");
    return StopResult::kDone;
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetWorldStream()
{
    return mManager.GetStream(Dia::Core::StringCRC("WLD"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetA1Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("A1"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetB4Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("B4"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetB6Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("B6"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetC1Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("C1"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetC2Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("C2"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetB5Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("B5"));
}

//-----------------------------------------------------------------------------
Dia::Maths::RandomStream* RandomStreamModule::GetC4Stream()
{
    return mManager.GetStream(Dia::Core::StringCRC("C4"));
}

} // namespace CoW
