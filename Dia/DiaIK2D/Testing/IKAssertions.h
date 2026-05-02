#pragma once

#include <gtest/gtest.h>
#include <cmath>

#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaIK2D/IKSolver.h>

namespace Dia
{
	namespace IK2D
	{
		namespace Testing
		{
			inline void AssertBoneRotation(const Dia::Rig2D::Pose& pose,
			                               int boneIndex,
			                               float expectedAngle,
			                               float toleranceRad)
			{
				const float actual = pose.GetLocalTransform(boneIndex).rotation;
				EXPECT_NEAR(actual, expectedAngle, toleranceRad)
				    << "Bone " << boneIndex << " rotation mismatch";
			}

			inline void AssertBoneUnchanged(const Dia::Rig2D::Pose& pose,
			                                int boneIndex,
			                                float snapshotRotation,
			                                float toleranceRad = 1e-4f)
			{
				const float actual = pose.GetLocalTransform(boneIndex).rotation;
				EXPECT_NEAR(actual, snapshotRotation, toleranceRad)
				    << "Bone " << boneIndex << " should not have changed";
			}

			inline void AssertEndEffectorPosition(IKSolver& solver,
			                                      const Dia::Rig2D::BoneTransform& root,
			                                      int boneIndex,
			                                      const Dia::Maths::Vector2D& expected,
			                                      float toleranceUnits)
			{
				solver.SetRootTransform(root);

				Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> worldTransforms;
				solver.GetPose().ComputeWorldTransforms(solver.GetSkeleton(), root, worldTransforms);

				const Dia::Maths::Vector2D& actual = worldTransforms[boneIndex].position;
				EXPECT_NEAR(actual.x, expected.x, toleranceUnits) << "End effector X mismatch";
				EXPECT_NEAR(actual.y, expected.y, toleranceUnits) << "End effector Y mismatch";
			}
		}
	}
}
