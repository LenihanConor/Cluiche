#include <Modules/Weather/WeatherSaveParticipant.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace CoW {
namespace Weather {

void WeatherSaveParticipant::Serialize(Dia::SaveGame::SaveContext& ctx) const
{
    DIA_TRACE_ZONE("WeatherSaveParticipant::Serialize", Dia::Observation::Trace::Category::kNone);
    Json::Value record(Json::objectValue);
    const Draoi::HexWorld::Weather::WeatherSimulation* sim = mStage.ReadableSim();
    const Draoi::HexWorld::BiomeRegistry* registry = mStage.GetRegistry();
    if (sim == nullptr || !sim->IsInitialised() || registry == nullptr
        || !EncodeRecord(*sim, mStage.ParametersHash(), *registry, record))
    {
        record = Json::Value(Json::objectValue);
        Draoi::HexWorld::WriteNotReady(record);
    }
    Json::Value& root = ctx.CurrentNode();   // the SaveManager opened this participant's object
    const Json::Value::Members names = record.getMemberNames();
    for (const std::string& name : names) { root[name] = record[name]; }
}

// All-or-nothing: decodes into a local, mutates no field, week, sim or queue. The one stage call is
// NoteCommittedRecord, which only remembers the record for ValidatePending (which runs before Restore).
bool WeatherSaveParticipant::Validate(Dia::SaveGame::LoadContext& ctx) const
{
    const Json::Value& root = ctx.Root();
    const bool notReady = root.isObject() && root["ready"].isBool() && !root["ready"].asBool();
    if (notReady)
    {
        mStage.NoteCommittedRecord(root);
        return true;
    }
    const Draoi::HexWorld::BiomeRegistry* registry = mStage.GetRegistry();
    if (!mConfigured || registry == nullptr)
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: saved weather rejected, the save spec or biome registry is not available");
        return false;
    }
    WeatherRecord rec;
    Draoi::HexWorld::SaveError err;
    if (!DecodeRecord(root, mSpec, *registry, rec, err))
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: saved weather rejected at '%s': %s", err.field.AsCStr(), err.reason.AsCStr());
        return false;
    }
    mStage.NoteCommittedRecord(root);
    return true;
}

void WeatherSaveParticipant::Deserialize(Dia::SaveGame::LoadContext& ctx)
{
    mStage.Restore(ctx.Root());
}

} // namespace Weather
} // namespace CoW
