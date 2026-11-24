// Spec: worldsim-kit.md section 4 (AC 18), plan task 30a.
// S-1 log channel renamed from "CoW" to "WGEN". Verified by capturing a log from WorldGenerationModule
// and checking that the channel is "WGEN".

#include <gtest/gtest.h>
#include <Modules/WorldGen/WorldGenerationModule.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>

#include <string>
#include <vector>

using namespace Dia::Core;
using namespace CoW;

namespace {

class LogCaptureSink : public Dia::Observation::Log::ISink
{
public:
    struct CapturedLog
    {
        Dia::Observation::Log::LogLevel level;
        Dia::Core::StringCRC channel;
        std::string message;
    };

    std::vector<CapturedLog> entries;

    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        entries.push_back({e.level, e.channel, std::string(e.message)});
    }

    const char* GetName() const override
    {
        return "LogCaptureSink";
    }

    bool HasChannelInMessage(const Dia::Core::StringCRC& expectedChannel, const char* messageHint = nullptr) const
    {
        for (const auto& entry : entries)
        {
            if (entry.channel == expectedChannel)
            {
                if (messageHint == nullptr || std::string(entry.message).find(messageHint) != std::string::npos)
                {
                    return true;
                }
            }
        }
        return false;
    }
};

}  // namespace

TEST(WorldGenLogChannelTest, RulesFailureLogsWithWGENChannel)
{
    LogCaptureSink sink;
    auto& logger = Dia::Observation::Log::Logger::Instance();
    logger.RegisterThreadBuffer();
    logger.RegisterSink(&sink);

    // Create module and trigger a rules load failure
    WorldGenerationModule wg(StringCRC("WorldGen"));
    wg.SetStrataPath(Draoi::HexWorld::Testing::ShippedStrataPath());
    wg.SetGuaranteesPath(CoW::WorldGeneration::Testing::ShippedGuaranteesPath());
    wg.SetBiomesPath("");  // Invalid path to trigger error
    wg.SetRulesSidecarText("");  // Empty text to trigger error

    // Call LoadRulesSidecar directly to trigger the log
    wg.LoadRulesSidecar();

    logger.FlushSync();

    // Check if we captured a WGEN channel log
    const Dia::Core::StringCRC wgenChannel("WGEN");
    bool found = sink.HasChannelInMessage(wgenChannel, "terrain rules sidecar");

    logger.UnregisterSink(&sink);
    logger.UnregisterThreadBuffer();

    // This should find at least one error log with WGEN channel
    EXPECT_TRUE(found) << "Expected to find a log entry with channel 'WGEN' containing 'terrain rules sidecar'";
}
