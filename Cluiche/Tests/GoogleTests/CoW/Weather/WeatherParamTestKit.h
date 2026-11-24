// Shared fixtures for the A4 weather parameter and queue tests (plan Task 4): the shipped sidecar text, a counting
// sidecar source, a hand-built world, a log sink that keeps the WTH entries in order, and a reference simulation.
#pragma once

#include <gtest/gtest.h>

#include <string.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Shared/CoWCalendar.h>
#include <Shared/Weather/WeatherStage.h>

#include "WeatherTestWorld.h"

namespace WeatherParamKit
{
namespace WT = Draoi::HexWorld::Weather::Testing;
using Draoi::HexWorld::Weather::Season;
using Draoi::HexWorld::Weather::WeatherSimulation;
using Draoi::HexWorld::Weather::WeatherTerrain;

constexpr uint32_t kSpinUpWeeks = 3;

inline std::string ReadShippedSidecar()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/Weather/weather_parameters.schema.json";
        if (std::filesystem::is_regular_file(candidate, ec))
        {
            std::ifstream in(candidate, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) { break; }
        dir = parent;
    }
    ADD_FAILURE() << "weather sidecar not found";
    return std::string();
}

inline const std::string& SidecarText() { static const std::string text = ReadShippedSidecar(); return text; }

inline Json::Value ParseJson(const std::string& text)
{
    Json::Value v;
    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream in(text);
    EXPECT_TRUE(Json::parseFromStream(b, in, &v, &errs)) << errs;
    return v;
}

inline std::string DumpJson(const Json::Value& v)
{
    Json::StreamWriterBuilder w;
    w["indentation"] = "";
    return Json::writeString(w, v);
}

// Serves the sidecar text and counts every read (AC 23). `fail` makes the next reads fail like a missing file.
class CountingSource : public CoW::Weather::IWeatherSidecarSource
{
public:
    explicit CountingSource(const std::string& t = SidecarText()) : text(t) {}
    bool Read(std::string& out, std::string& error) override
    {
        ++reads;
        if (fail) { error = "cannot read weather_parameters.schema.json (test)"; return false; }
        out = text;
        return true;
    }
    std::string text;
    bool fail = false;
    uint32_t reads = 0;
};

class FakeWorldSource : public CoW::Weather::IWeatherWorldSource
{
public:
    bool GetWorld(CoW::WorldStep::WorldView& view, WeatherTerrain& terrain) override
    {
        WT::HexTerrainOptions o;
        o.waterColumns = 2;
        o.latitudeRamp = true;
        WeatherTestWorld::CopyTerrain(WT::MakeHexTerrain(8, 6, o), terrain);
        view.present = true;
        view.width = 8;
        view.height = 6;
        return true;
    }
};

struct FakeHold : public CoW::WorldStep::IGenerationHold
{
    void Hold() override {}
    void Release() override {}
};

// Keeps every WTH log entry in arrival order.
class WthLog : public Dia::Observation::Log::ISink
{
public:
    struct Line { Dia::Observation::Log::LogLevel level; std::string text; };
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (strstr(e.message, "WTH") == nullptr) { return; }
        lines.push_back(Line{ e.level, e.message });
    }
    const char* GetName() const override { return "WeatherParamKitSink"; }
    unsigned Count(Dia::Observation::Log::LogLevel level, const char* needle) const
    {
        unsigned n = 0;
        for (const Line& l : lines) { if (l.level == level && l.text.find(needle) != std::string::npos) { ++n; } }
        return n;
    }
    int IndexOf(const char* needle) const
    {
        for (size_t i = 0; i < lines.size(); ++i) { if (lines[i].text.find(needle) != std::string::npos) { return static_cast<int>(i); } }
        return -1;
    }
    std::vector<Line> lines;
};

// The reference run: a direct Initialise + Step loop with the stage's calendar written out by hand.
struct RefSim
{
    RefSim(const Draoi::HexWorld::Weather::WeatherParameters& p, uint32_t weeks = kSpinUpWeeks) : params(p)
    {
        FakeWorldSource src;
        CoW::WorldStep::WorldView view;
        src.GetWorld(view, terrain);
        EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry(), nullptr));
        for (uint32_t i = 0; i < weeks; ++i)
        {
            sim.Step(params, static_cast<Season>((i / 13u) % 4u), i % 52u, WT::TestRegistry(), nullptr, noHydro, noInfl, exec);
        }
    }
    void Live(uint64_t day)
    {
        sim.Step(params, static_cast<Season>((day / (7u * 13u)) % 4u), static_cast<uint32_t>((day / 7u) % 52u),
                 WT::TestRegistry(), nullptr, noHydro, noInfl, exec);
        sim.SetCommittedWeek(day / 7u);
    }
    WeatherTerrain terrain;
    Draoi::HexWorld::Weather::WeatherParameters params;
    WeatherSimulation sim;
    Draoi::HexWorld::Weather::HydrologyInputs noHydro;
    Draoi::HexWorld::Weather::InfluenceSnapshot noInfl;
    Dia::ParallelExecutor::SerialExecutor exec;
};

// A stage with the shipped sidecar loaded through a counting source, a hand-built world and manual job launching.
class ParamStageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        l.RegisterSink(&log);
        stage.SetWorldSource(&world);
        stage.SetRegistry(&WT::TestRegistry());
        stage.SetSpinupWeeks(kSpinUpWeeks);
        stage.SetGenerationHold(&hold);
        stage.SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kManual);
        stage.Start();
    }
    void TearDown() override
    {
        stage.Stop();
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.FlushSync();
        l.UnregisterSink(&log);
        l.UnregisterThreadBuffer();
    }
    bool Load() { return stage.LoadParameters(source, {}, err); }
    void SpinUp(uint32_t revision)
    {
        stage.OnWorldComplete(revision);
        while (stage.RunNextJob()) {}
        stage.Update();
    }
    void Step(uint64_t day)
    {
        stage.Capture(static_cast<Dia::DeferredStep::StepKey>(day));
        stage.Run(exec);
        stage.Commit();
    }
    const WthLog& Log()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        return log;
    }
    Draoi::HexWorld::Weather::WeatherParameters Snapshot() const
    {
        Draoi::HexWorld::Weather::WeatherParameters p;
        stage.Parameters().Snapshot(p);
        return p;
    }
    bool Matches(const RefSim& ref) const
    {
        const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
        return snap.fields != nullptr && WT::SameFields(ref.sim.Fields(), *snap.fields) && ref.sim.GetCommittedWeek() == snap.week;
    }
    bool Health(Dia::Observation::Health::HealthStatus status) const { return stage.GetHealth().status == status; }

    CountingSource source;
    FakeWorldSource world;
    FakeHold hold;
    WthLog log;
    CoW::Weather::WeatherStage stage;
    CoW::Weather::ParamError err;
    Dia::ParallelExecutor::SerialExecutor exec;
};

} // namespace WeatherParamKit
