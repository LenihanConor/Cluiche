#pragma once
// A4 weather-save: the ISaveable for the committed weather record (id cow.weather, version 1).
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-save.md. Thin: the record codec is
// Shared/Weather/WeatherSaveFields, the restore sequence is WorldStepStageBase::Restore. Registration with
// SaveLoadModule belongs to the owning module (plan Task 12).
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <Shared/Weather/WeatherSaveFields.h>
#include <Shared/Weather/WeatherStage.h>

namespace CoW {
namespace Weather {

class WeatherSaveParticipant final : public Dia::SaveGame::ISaveable
{
public:
    explicit WeatherSaveParticipant(WeatherStage& stage) : mStage(stage) {}
    WeatherSaveParticipant(const WeatherSaveParticipant&) = delete;
    WeatherSaveParticipant& operator=(const WeatherSaveParticipant&) = delete;

    // Resolves the ten field ranges from the sidecar schema (ValueSchemaRegistry or ValueSchemaService) and hands the
    // record spec to the stage. Until it succeeds Validate accepts only a ready:false record.
    template <class TSchema>
    bool ConfigureSchema(const TSchema& schema, Draoi::HexWorld::SaveError& err)
    {
        if (!ResolveWeatherSaveFields(schema, mFields, err)) { return false; }
        mSpec = WeatherSaveRecordSpec(mFields);
        mConfigured = true;
        mStage.SetSaveSpec(&mSpec);
        return true;
    }
    bool IsConfigured() const { return mConfigured; }

    void     Serialize(Dia::SaveGame::SaveContext& ctx) const override;
    bool     Validate(Dia::SaveGame::LoadContext& ctx) const override;
    void     Deserialize(Dia::SaveGame::LoadContext& ctx) override;
    uint32_t GetVersion() const override { return 1; }

private:
    WeatherStage& mStage;
    Draoi::HexWorld::SavedField mFields[kWeatherSaveFieldCount];
    WorldStep::RecordSpec mSpec;
    bool mConfigured = false;
};

} // namespace Weather
} // namespace CoW
