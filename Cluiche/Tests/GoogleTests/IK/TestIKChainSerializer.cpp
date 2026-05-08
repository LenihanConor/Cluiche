#include <gtest/gtest.h>

#include <DiaIK2D/JsonIKChainSerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <cstdio>
#include <string>

using namespace Dia::IK2D;

static const char* kValidChain = R"({
    "id": "arm_chain",
    "start_bone": "shoulder",
    "end_bone": "wrist",
    "reach_weight": 0.9,
    "max_iterations": 15,
    "tolerance": 0.005,
    "joint_limits": [
        { "min_angle": -1.5, "max_angle": 1.5, "enabled": true },
        { "min_angle": -0.5, "max_angle": 0.5, "enabled": true }
    ]
})";

static const char* kValidMinimal = R"({
    "id": "leg",
    "start_bone": "hip",
    "end_bone": "ankle"
})";

static const char* kMissingId = R"({
    "start_bone": "a",
    "end_bone": "b"
})";

static const char* kMissingStartBone = R"({
    "id": "x",
    "end_bone": "b"
})";

static const char* kMalformed = R"({ not json )";

static const char* kTmpPath = "C:\\Temp\\dia_test_ikchain.json";

TEST(IKChainSerializer, Load_ValidFull)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    ASSERT_TRUE(s.Load(kValidChain, def));

    EXPECT_EQ(def.id, Dia::Core::StringCRC("arm_chain"));
    EXPECT_EQ(def.startBoneId, Dia::Core::StringCRC("shoulder"));
    EXPECT_EQ(def.endBoneId, Dia::Core::StringCRC("wrist"));
    EXPECT_FLOAT_EQ(def.reachWeight, 0.9f);
    EXPECT_EQ(def.maxIterations, 15);
    EXPECT_FLOAT_EQ(def.tolerance, 0.005f);
    EXPECT_EQ(def.jointLimits.Size(), 2u);
    EXPECT_TRUE(def.jointLimits[0].enabled);
    EXPECT_FLOAT_EQ(def.jointLimits[0].minAngle, -1.5f);
    EXPECT_FLOAT_EQ(def.jointLimits[0].maxAngle,  1.5f);
}

TEST(IKChainSerializer, Load_DefaultsApplied)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    ASSERT_TRUE(s.Load(kValidMinimal, def));

    EXPECT_EQ(def.id, Dia::Core::StringCRC("leg"));
    EXPECT_FLOAT_EQ(def.reachWeight, 1.0f);
    EXPECT_EQ(def.maxIterations, 20);
    EXPECT_FLOAT_EQ(def.tolerance, 0.001f);
    EXPECT_EQ(def.jointLimits.Size(), 0u);
}

TEST(IKChainSerializer, Load_MissingId_Fails)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    auto result = s.Load(kMissingId, def);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(IKChainSerializer, Load_MissingStartBone_Fails)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    EXPECT_FALSE(s.Load(kMissingStartBone, def));
}

TEST(IKChainSerializer, Load_MalformedJson_Fails)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    auto result = s.Load(kMalformed, def);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
    EXPECT_GT(strlen(result.error), 0u);
}

TEST(IKChainSerializer, RoundTrip)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    ASSERT_TRUE(s.Load(kValidChain, def));

    char buffer[4096];
    ASSERT_TRUE(s.Save(def, buffer, sizeof(buffer)));

    IKChainDef def2;
    ASSERT_TRUE(s.Load(buffer, def2));

    EXPECT_EQ(def2.id, def.id);
    EXPECT_EQ(def2.startBoneId, def.startBoneId);
    EXPECT_EQ(def2.endBoneId, def.endBoneId);
    EXPECT_FLOAT_EQ(def2.reachWeight, def.reachWeight);
    EXPECT_EQ(def2.maxIterations, def.maxIterations);
    EXPECT_NEAR(def2.tolerance, def.tolerance, 0.0001f);
    EXPECT_EQ(def2.jointLimits.Size(), def.jointLimits.Size());
    for (unsigned int i = 0; i < def.jointLimits.Size(); ++i)
    {
        EXPECT_FLOAT_EQ(def2.jointLimits[i].minAngle, def.jointLimits[i].minAngle);
        EXPECT_FLOAT_EQ(def2.jointLimits[i].maxAngle, def.jointLimits[i].maxAngle);
        EXPECT_EQ(def2.jointLimits[i].enabled, def.jointLimits[i].enabled);
    }
}

TEST(IKChainSerializer, SaveToFileAndLoadFromFile)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    ASSERT_TRUE(s.Load(kValidChain, def));

    ASSERT_TRUE(s.SaveToFile(kTmpPath, def));

    IKChainDef loaded;
    ASSERT_TRUE(s.LoadFromFile(kTmpPath, loaded));

    EXPECT_EQ(loaded.id, def.id);
    EXPECT_EQ(loaded.startBoneId, def.startBoneId);
    EXPECT_EQ(loaded.endBoneId, def.endBoneId);
    EXPECT_EQ(loaded.jointLimits.Size(), def.jointLimits.Size());

    remove(kTmpPath);
}

TEST(IKChainSerializer, LoadFromFile_Missing_Fails)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    auto result = s.LoadFromFile("C:\\Temp\\nonexistent_ikchain.json", def);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(IKChainSerializer, GetVersion_NotEmpty)
{
    JsonIKChainSerializer s;
    const char* v = s.GetVersion();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(strlen(v), 0u);
}

TEST(IKChainSerializer, Save_BufferTooSmall_Fails)
{
    JsonIKChainSerializer s;
    IKChainDef def;
    ASSERT_TRUE(s.Load(kValidChain, def));

    char tiny[1];
    auto result = s.Save(def, tiny, sizeof(tiny));
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(IKChainSerializer, Load_JointLimitsAtCapacity)
{
    JsonIKChainSerializer s;

    // Build a JSON with exactly kMaxJointLimits entries
    std::string json = R"({"id":"cap","start_bone":"a","end_bone":"b","joint_limits":[)";
    for (unsigned int i = 0; i < Dia::IK2D::kMaxJointLimits; ++i)
    {
        if (i > 0) json += ",";
        json += R"({"min_angle":-1.0,"max_angle":1.0,"enabled":true})";
    }
    json += "]}";

    IKChainDef def;
    ASSERT_TRUE(s.Load(json.c_str(), def));
    EXPECT_EQ(def.jointLimits.Size(), Dia::IK2D::kMaxJointLimits);
}

TEST(IKChainSerializer, Load_JointLimitsBeyondCapacity_Truncates)
{
    JsonIKChainSerializer s;

    // Build a JSON with more than kMaxJointLimits entries
    std::string json = R"({"id":"cap","start_bone":"a","end_bone":"b","joint_limits":[)";
    for (unsigned int i = 0; i < Dia::IK2D::kMaxJointLimits + 5; ++i)
    {
        if (i > 0) json += ",";
        json += R"({"min_angle":-1.0,"max_angle":1.0,"enabled":false})";
    }
    json += "]}";

    IKChainDef def;
    ASSERT_TRUE(s.Load(json.c_str(), def));
    EXPECT_EQ(def.jointLimits.Size(), Dia::IK2D::kMaxJointLimits);
}
