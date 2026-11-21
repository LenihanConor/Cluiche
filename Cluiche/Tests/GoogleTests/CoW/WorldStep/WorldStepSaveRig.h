#pragma once
// Rig for the World Step save/load tests (Feature 3). Test-only, not shipped.
//
// A Rig (real WorldStepModule + real DeferredStepScheduler over the manual launcher + two fake stages) plus a real
// SaveManager/SaveRegistry on a temp slot, the real DeferredStepSaveParticipant, and a stand-in for each stage's own
// committed-state participant (A4 WeatherSaveParticipant et al). Real jsoncpp text goes to disk and back.
//
// Worlds are used one at a time: a module registers process-wide metrics and a health reporter, so a saving world is
// destroyed before the loading world is created. The slot file outlives the world that wrote it.
#include "WorldStepTestRig.h"

#include <DiaDeferredStep/DeferredStepSaveParticipant.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

#include <direct.h>
#include <cstdio>
#include <string>

namespace WsTest
{
    using CoW::WorldStep::Testing::FakeWorldStepListener;

    inline std::string TempDir() { return "temp/worldstep_save_tests/"; }

    // Short unique slot id per test (gtest shards run as parallel processes sharing the temp directory).
    inline std::string UniqueSlot(const char* tag = "")
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string full = std::string(info->test_suite_name()) + "." + info->name() + tag;
        char buf[24];
        std::snprintf(buf, sizeof(buf), "ws%08x", static_cast<unsigned>(StringCRC{ full.c_str() }.Value()));
        return buf;
    }

    // Removes the slot file when the test ends (and before it starts, if a crashed run left one).
    struct SlotFile
    {
        explicit SlotFile(const std::string& slot) : path(TempDir() + "slot_" + slot + ".sav") { std::remove(path.c_str()); }
        ~SlotFile() { std::remove(path.c_str()); }
        std::string path;
    };

    // Stand-in for the stages' own committed-state participants.
    class StageStatePart final : public Dia::SaveGame::ISaveable
    {
    public:
        StageStatePart(FakeWorldStepStage& a, FakeWorldStepStage& b) : mA(a), mB(b) {}
        void Serialize(Dia::SaveGame::SaveContext& ctx) const override
        {
            ctx.Write(StringCRC("weather"), static_cast<int32_t>(mA.committed));
            ctx.Write(StringCRC("ecology"), static_cast<int32_t>(mB.committed));
        }
        void Deserialize(Dia::SaveGame::LoadContext& ctx) override
        {
            ++deserializeCalls;
            int32_t v = 0;
            if (ctx.Read(StringCRC("weather"), v)) mA.committed = v;
            if (ctx.Read(StringCRC("ecology"), v)) mB.committed = v;
        }
        uint32_t GetVersion() const override { return 1; }
        uint32_t deserializeCalls = 0;
    private:
        FakeWorldStepStage& mA;
        FakeWorldStepStage& mB;
    };

    struct SaveRig : Rig
    {
        // stateFirst = true: the stage-state participant is registered before the DeferredStep participant (the contract).
        explicit SaveRig(const std::string& slotName, const std::string& config = kDefaultConfig,
                         bool registerStages = true, bool stateFirst = true)
            : Rig(config, registerStages)
            , slotId(slotName.c_str())
            , saveConfig{ "temp/worldstep_save_tests/", "slot_{id}.sav", 0, Dia::SaveGame::SaveFormat::Json }
            , state(weather, ecology)
            , participant(scheduler)
        {
            _mkdir("temp");
            _mkdir("temp/worldstep_save_tests");
            manager.Init(saveConfig, registry);
            if (stateFirst)
            {
                registry.Register(StringCRC("WsStageState"), &state);
                participant.Attach(registry, manager);
            }
            else
            {
                participant.Attach(registry, manager);
                registry.Register(StringCRC("WsStageState"), &state);
            }
        }

        ~SaveRig()
        {
            registry.Unregister(StringCRC("WsStageState"));
            // participant detaches in its destructor, before registry and manager are destroyed
        }

        Dia::SaveGame::SaveResult Save() { return manager.Save(slotId); }
        Dia::SaveGame::LoadResult Load() { return manager.Load(slotId); }

        StringCRC slotId;
        Dia::SaveGame::SaveConfig saveConfig;
        Dia::SaveGame::SaveRegistry registry;
        Dia::SaveGame::SaveManager manager;
        StageStatePart state;
        Dia::DeferredStep::DeferredStepSaveParticipant participant;
    };

    // Each day's tick with a day-specific input, so a restored result that is wrong cannot match by accident.
    inline void Drive(Rig& r, uint64_t first, uint64_t last)
    {
        for (uint64_t d = first; d <= last; ++d)
        {
            r.weather.source = static_cast<int>(d);
            r.ecology.source = static_cast<int>(d * 2);
            r.Step(d);
        }
    }

    struct StageSnapshot
    {
        int working, result, committed;
        bool operator==(const StageSnapshot& o) const { return working == o.working && result == o.result && committed == o.committed; }
    };
    inline StageSnapshot Snap(const FakeWorldStepStage& s) { return { s.working, s.result, s.committed }; }
}
