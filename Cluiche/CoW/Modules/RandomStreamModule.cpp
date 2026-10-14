#include "RandomStreamModule.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

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
    // Seed for kStreamNames[i] is (i + 1) * 10000 (WLD = world generation).
    for (int i = 0; i < kStreamCount; ++i)
        mManager.CreateStream(Dia::Core::StringCRC(kStreamNames[i]), static_cast<unsigned int>((i + 1) * 10000));

    DIA_LOG_INFO("CoW", "RandomStreamModule: Created 8 independent RNG streams for deterministic simulation");

    return StartResult::kReady;
}

//-----------------------------------------------------------------------------
Dia::ApplicationFlow::StopResult RandomStreamModule::DoStop()
{
    using Dia::ApplicationFlow::StopResult;

    UnregisterSaveParticipant();
    // Manager destructor will clean up all streams
    DIA_LOG_INFO("CoW", "RandomStreamModule: Shutting down RNG streams");
    return StopResult::kDone;
}

//-----------------------------------------------------------------------------
void RandomStreamModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    TryRegisterSaveParticipant();
}

//-----------------------------------------------------------------------------
void RandomStreamModule::TryRegisterSaveParticipant()
{
    if (mSaveParticipantRegistered)
        return;
    if (SaveLoadModule* sl = mSaveLoad.Get())
    {
        sl->RegisterParticipant(Dia::Core::StringCRC("random_streams"), &mSaveParticipant);
        mSaveParticipantRegistered = true;
        DIA_LOG_INFO("CoW", "RandomStreamModule: save participant 'random_streams' registered");
    }
}

//-----------------------------------------------------------------------------
void RandomStreamModule::UnregisterSaveParticipant()
{
    if (!mSaveParticipantRegistered)
        return;
    if (SaveLoadModule* sl = mSaveLoad.Get())
        sl->UnregisterParticipant(Dia::Core::StringCRC("random_streams"));
    mSaveParticipantRegistered = false;
}

//-----------------------------------------------------------------------------
void RandomStreamSaveParticipant::Serialize(Dia::SaveGame::SaveContext& ctx) const
{
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
    {
        const Dia::Core::StringCRC name(RandomStreamModule::kStreamNames[i]);
        const Dia::Maths::RandomStream* stream = mModule.GetStreamByName(name);
        if (!stream)
            continue;
        ctx.BeginObject(name);
        ctx.Write(Dia::Core::StringCRC("seed"), static_cast<int64_t>(stream->GetSeed()));
        ctx.Write(Dia::Core::StringCRC("draws"), static_cast<int64_t>(stream->GetDrawCount()));
        ctx.EndObject();
    }
}

//-----------------------------------------------------------------------------
void RandomStreamSaveParticipant::Deserialize(Dia::SaveGame::LoadContext& ctx)
{
    for (int i = 0; i < RandomStreamModule::kStreamCount; ++i)
    {
        const char* nameStr = RandomStreamModule::kStreamNames[i];
        const Dia::Core::StringCRC name(nameStr);
        if (!ctx.BeginObject(name))
        {
            DIA_LOG_WARNING("CoW", "RandomStreamSaveParticipant: stream '%s' missing from save, left unchanged", nameStr);
            continue;
        }
        int64_t seed = 0;
        int64_t draws = 0;
        const bool ok = ctx.Read(Dia::Core::StringCRC("seed"), seed)
                     && ctx.Read(Dia::Core::StringCRC("draws"), draws);
        ctx.EndObject();
        if (!ok || seed < 0 || draws < 0)
        {
            DIA_LOG_ERROR("CoW", "RandomStreamSaveParticipant: stream '%s' has missing or invalid seed/draws, left unchanged", nameStr);
            continue;
        }
        if (Dia::Maths::RandomStream* stream = mModule.GetStreamByName(name))
            stream->Restore(static_cast<unsigned int>(seed), static_cast<uint64_t>(draws));
    }
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
