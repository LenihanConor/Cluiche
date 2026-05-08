#pragma once
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <gtest/gtest.h>
#include <cmath>

namespace Dia { namespace Animation2D { namespace Testing {

    inline float ShortestArcDiff(float a, float b) {
        const float kPi = 3.14159265358979f;
        float diff = b - a;
        while (diff > kPi)  diff -= 2.0f * kPi;
        while (diff < -kPi) diff += 2.0f * kPi;
        return std::abs(diff);
    }

    // Returns true if the two transforms are equal within tolerance.
    // Use AssertBoneTransformNear (EXPECT-based) to register test failures.
    inline bool BoneTransformNear(
        const Dia::Rig2D::BoneTransform& expected,
        const Dia::Rig2D::BoneTransform& actual,
        float tolerance = 1e-5f)
    {
        return std::abs(expected.position.x - actual.position.x) < tolerance
            && std::abs(expected.position.y - actual.position.y) < tolerance
            && ShortestArcDiff(expected.rotation, actual.rotation) < tolerance
            && std::abs(expected.scale.x - actual.scale.x) < tolerance
            && std::abs(expected.scale.y - actual.scale.y) < tolerance;
    }

    // Registers EXPECT failures for each bone that differs beyond tolerance.
    inline void AssertBoneTransformNear(
        const Dia::Rig2D::BoneTransform& expected,
        const Dia::Rig2D::BoneTransform& actual,
        float tolerance = 1e-5f)
    {
        EXPECT_NEAR(expected.position.x, actual.position.x, tolerance);
        EXPECT_NEAR(expected.position.y, actual.position.y, tolerance);
        EXPECT_LT(ShortestArcDiff(expected.rotation, actual.rotation), tolerance);
        EXPECT_NEAR(expected.scale.x, actual.scale.x, tolerance);
        EXPECT_NEAR(expected.scale.y, actual.scale.y, tolerance);
    }

    // Checks that two poses are equal within tolerance.
    // Does NOT register GoogleTest failures — safe to call without affecting the
    // current test result. Returns true if all bones match.
    inline bool AssertPosesEqual(
        const Dia::Rig2D::Pose& expected,
        const Dia::Rig2D::Pose& actual,
        float tolerance = 1e-5f)
    {
        if (expected.GetBoneCount() != actual.GetBoneCount()) return false;
        bool allMatch = true;
        for (int i = 0; i < expected.GetBoneCount(); ++i) {
            if (!BoneTransformNear(expected.GetLocalTransform(i), actual.GetLocalTransform(i), tolerance))
                allMatch = false;
        }
        return allMatch;
    }

    // Registers an EXPECT_LT failure if the bone rotation differs beyond tolerance.
    inline void AssertBoneRotation(
        const Dia::Rig2D::Pose& pose,
        int boneIndex,
        float expectedRotation,
        float tolerance = 1e-5f)
    {
        EXPECT_LT(ShortestArcDiff(expectedRotation, pose.GetLocalTransform(boneIndex).rotation), tolerance);
    }

} } }
