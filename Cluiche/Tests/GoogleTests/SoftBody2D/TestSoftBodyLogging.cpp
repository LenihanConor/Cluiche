#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogEntry.h>
#include <DiaLogger/LogLevel.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

class SoftBodyLogSink : public Dia::Logger::ISink
{
public:
    static const unsigned int kMaxEntries = 128;

    SoftBodyLogSink()
        : mEntryCount(0)
    {
        SetLevelThreshold(Dia::Logger::LogLevel::kDebug);
        SetChannelFilter(Dia::Core::StringCRC("Physics"), true);
    }

    void OnLogEntry(const Dia::Logger::LogEntry& entry) override
    {
        if (mEntryCount < kMaxEntries)
            mEntries[mEntryCount++] = entry;
    }

    const char* GetName() const override { return "SoftBodyLogSink"; }

    unsigned int GetEntryCount() const { return mEntryCount; }
    const Dia::Logger::LogEntry& GetEntry(unsigned int i) const { return mEntries[i]; }

    void Clear() { mEntryCount = 0; }

    unsigned int CountByLevel(Dia::Logger::LogLevel level) const
    {
        unsigned int count = 0;
        for (unsigned int i = 0; i < mEntryCount; ++i)
        {
            if (mEntries[i].level == level) ++count;
        }
        return count;
    }

    bool HasMessageContaining(const char* substring) const
    {
        for (unsigned int i = 0; i < mEntryCount; ++i)
        {
            if (strstr(mEntries[i].message, substring) != nullptr)
                return true;
        }
        return false;
    }

private:
    Dia::Logger::LogEntry mEntries[kMaxEntries];
    unsigned int mEntryCount;
};

class SoftBodyLoggingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSink.Clear();
        Dia::Logger::Logger::Instance().RegisterThreadBuffer();
        Dia::Logger::Logger::Instance().RegisterSink(&mSink);
    }

    void TearDown() override
    {
        Dia::Logger::Logger::Instance().UnregisterSink(&mSink);
        Dia::Logger::Logger::Instance().UnregisterThreadBuffer();
    }

    void FlushLogs()
    {
        Dia::Logger::Logger::Instance().FlushBuffers();
    }

    SoftBodyLogSink mSink;
};

#ifndef NDEBUG

TEST_F(SoftBodyLoggingTest, MaxSubSteps_WarningEmitted)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 2;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("LogRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    world.AddRope(rdef);

    world.Update(10.0f);
    FlushLogs();

    EXPECT_GE(mSink.CountByLevel(Dia::Logger::LogLevel::kWarning), 1u);
    EXPECT_TRUE(mSink.HasMessageContaining("maxSubSteps"));
}

TEST_F(SoftBodyLoggingTest, MaxSubSteps_MessageContainsDeltaTime)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 2;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("DeltaRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(1.0f, 0.0f);
    rdef.particleCount = 2;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    world.AddRope(rdef);

    world.Update(10.0f);
    FlushLogs();

    EXPECT_TRUE(mSink.HasMessageContaining("deltaTime"));
}

TEST_F(SoftBodyLoggingTest, NoMaxSubStepsWarning_OnNormalDelta)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("NormalRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(1.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    world.AddRope(rdef);

    world.Update(1.0f / 60.0f);
    FlushLogs();

    bool hasMaxSubStepsWarning = false;
    for (unsigned int i = 0; i < mSink.GetEntryCount(); ++i)
    {
        if (mSink.GetEntry(i).level == Dia::Logger::LogLevel::kWarning &&
            strstr(mSink.GetEntry(i).message, "maxSubSteps") != nullptr)
        {
            hasMaxSubStepsWarning = true;
        }
    }
    EXPECT_FALSE(hasMaxSubStepsWarning);
}

TEST_F(SoftBodyLoggingTest, VelocityThreshold_WarningOnExtremeDisplacement)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 1;
    def.solverIterations = 1;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("FastRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(1.0f, 0.0f);
    rdef.particleCount = 2;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);

    rope->GetParticle(0).position = Vector2D(1000.0f, 0.0f);

    world.Update(def.fixedTimestep);
    FlushLogs();

    EXPECT_TRUE(mSink.HasMessageContaining("derived velocity"));
    EXPECT_TRUE(mSink.HasMessageContaining("exceeds safety threshold"));
}

TEST_F(SoftBodyLoggingTest, VelocityWarning_ContainsBodyIdAndParticleIndex)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 1;
    def.solverIterations = 1;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("VelRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(1.0f, 0.0f);
    rdef.particleCount = 2;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);

    rope->GetParticle(0).position = Vector2D(1000.0f, 0.0f);

    world.Update(def.fixedTimestep);
    FlushLogs();

    EXPECT_TRUE(mSink.HasMessageContaining("particle ["));
}

TEST_F(SoftBodyLoggingTest, NoVelocityWarning_OnStableSimulation)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("StableVelRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -2.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);
    FlushLogs();

    bool hasVelocityWarning = false;
    for (unsigned int i = 0; i < mSink.GetEntryCount(); ++i)
    {
        if (strstr(mSink.GetEntry(i).message, "derived velocity") != nullptr)
            hasVelocityWarning = true;
    }
    EXPECT_FALSE(hasVelocityWarning);
}

TEST_F(SoftBodyLoggingTest, TearingDebugLog_EmittedOnTear)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -100.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("TearRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 0.1f;
    rdef.stiffness = 0.01f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.001f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);
    FlushLogs();

    EXPECT_TRUE(rope->IsTorn());
    EXPECT_TRUE(mSink.HasMessageContaining("torn at stretch ratio"));
}

TEST_F(SoftBodyLoggingTest, TearingMessage_ContainsBodyIdAndConstraintIndex)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -100.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("TearIdRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 0.1f;
    rdef.stiffness = 0.01f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.001f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);
    FlushLogs();

    EXPECT_TRUE(mSink.HasMessageContaining("constraint ["));
}

TEST_F(SoftBodyLoggingTest, NoTearingLog_ForNonTearableRope)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -100.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("NoTearRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -2.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);
    FlushLogs();

    EXPECT_FALSE(mSink.HasMessageContaining("torn"));
}

TEST_F(SoftBodyLoggingTest, StableSimulation_ZeroWarnings)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("StableCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 4;
    cdef.resY = 4;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.05f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    world.AddCloth(cdef);

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);
    FlushLogs();

    EXPECT_EQ(mSink.CountByLevel(Dia::Logger::LogLevel::kWarning), 0u);
}

#endif // NDEBUG
