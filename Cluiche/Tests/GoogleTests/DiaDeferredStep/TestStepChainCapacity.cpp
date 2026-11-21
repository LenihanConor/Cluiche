// StepChain capacity (AC 2) and StepChainApplied payload (AC 3).
#include <gtest/gtest.h>
#include <DiaDeferredStep/StepChain.h>
#include <DiaDeferredStep/StepChainConfigLoader.h>
#include <DiaDeferredStep/Testing/FakeStepStage.h>

#include <DiaDeferredStep/SerialExecutor.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::DeferredStep;
using namespace Dia::DeferredStep::Testing;
using Dia::Core::StringCRC;

namespace
{
    template <uint32_t Capacity>
    void ExpectCapacityHonoured()
    {
        static_assert(StepChain<Capacity>::RegisteredTableCapacity() == Capacity, "registered table size");
        static_assert(StepChain<Capacity>::EnabledTableCapacity() == Capacity, "enabled table size");

        CallLog log;
        std::vector<std::unique_ptr<FakeStepStage>> stages;
        std::vector<std::string> names;
        names.reserve(Capacity + 1);
        StepChainConfig<Capacity> cfg;
        for (uint32_t i = 0; i < Capacity; ++i)
        {
            names.push_back("stage" + std::to_string(i));
            StageId id;
            id.crc = StringCRC(names.back().c_str());
            id.name = Dia::Core::Containers::String32(names.back().c_str());
            cfg.order.Add(id);
        }

        StepChain<Capacity> chain;
        ASSERT_TRUE(chain.Configure(cfg));
        for (uint32_t i = 0; i < Capacity; ++i)
        {
            stages.push_back(std::make_unique<FakeStepStage>(StringCRC(names[i].c_str()), log));
            EXPECT_TRUE(chain.RegisterStage(StringCRC(names[i].c_str()), *stages.back())) << i;
        }
        EXPECT_EQ(chain.RegisteredCount(), Capacity);
        EXPECT_EQ(chain.EnabledCount(), Capacity);

        names.push_back("overflow");
        FakeStepStage extra(StringCRC("overflow"), log);
        EXPECT_FALSE(chain.RegisterStage(StringCRC("overflow"), extra));
        EXPECT_EQ(chain.RegisteredCount(), Capacity);

        SerialExecutor exec;
        ASSERT_TRUE(chain.Capture(1));
        ASSERT_TRUE(chain.Run(exec));
        ASSERT_TRUE(chain.Commit());
        EXPECT_EQ(log.Size(), Capacity * 3u);
    }
}

TEST(StepChainCapacity, Capacity2RegistersTwoAndRejectsThird) { ExpectCapacityHonoured<2>(); }
TEST(StepChainCapacity, Capacity8RegistersEightAndRejectsNinth) { ExpectCapacityHonoured<8>(); }
TEST(StepChainCapacity, Capacity16RegistersSixteenAndRejectsSeventeenth) { ExpectCapacityHonoured<16>(); }

TEST(StepChainCapacity, DefaultCapacityIsEight)
{
    EXPECT_EQ(StepChain<>::RegisteredTableCapacity(), 8u);
    EXPECT_EQ(kDefaultStageCapacity, 8u);
}

TEST(StepChainCapacity, ConfigLoaderRejectsMoreIdsThanCapacity)
{
    Json::Value root(Json::objectValue);
    root["order"] = Json::Value(Json::arrayValue);
    root["order"].append("a");
    root["order"].append("b");
    root["order"].append("c");

    StepChainConfig<2> small;
    ConfigError err;
    EXPECT_FALSE(LoadStepChainConfig(root, small, err));
    EXPECT_STREQ(err.key.AsCStr(), "order");

    StepChainConfig<3> fits;
    EXPECT_TRUE(LoadStepChainConfig(root, fits, err));
    EXPECT_EQ(fits.order.Size(), 3u);
}

TEST(StepChainApplied, CarriesApplyKeyStepKeyAndStageCount)
{
    FakeStepChainListener listener;
    StepChainApplied applied;
    applied.applyKey = 12;
    applied.stepKey = 5;
    applied.stageCount = 3;
    static_cast<IStepChainListener&>(listener).OnStepChainApplied(applied);

    ASSERT_EQ(listener.received.Size(), 1u);
    EXPECT_EQ(listener.received[0].applyKey, 12u);
    EXPECT_EQ(listener.received[0].stepKey, 5u);
    EXPECT_EQ(listener.received[0].stageCount, 3u);
}

TEST(StepChainApplied, DefaultsToZero)
{
    const StepChainApplied applied;
    EXPECT_EQ(applied.applyKey, 0u);
    EXPECT_EQ(applied.stepKey, 0u);
    EXPECT_EQ(applied.stageCount, 0u);
}
