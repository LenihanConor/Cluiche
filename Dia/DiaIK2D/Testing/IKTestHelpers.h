#pragma once

#include <memory>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaIK2D/IKSolver.h>

namespace Dia
{
	namespace IK2D
	{
		namespace Testing
		{
			struct TestRig
			{
				Dia::Rig2D::Skeleton skeleton;
				Dia::Rig2D::Pose     pose;

				TestRig(const Dia::Rig2D::SkeletonDef& def)
					: skeleton(def)
					, pose(skeleton)
				{
				}
			};

			// Builds a straight-line limb skeleton on the heap. Bone 0 is the root; each
			// subsequent bone is boneLength units along +Y from its parent.
			inline std::unique_ptr<Dia::Rig2D::SkeletonDef> MakeLimbSkeletonDef(int boneCount, float boneLength)
			{
				auto def = std::make_unique<Dia::Rig2D::SkeletonDef>();
				def->id = Dia::Core::StringCRC("TestLimb");

				for (int i = 0; i < boneCount; ++i)
				{
					Dia::Rig2D::Bone bone;

					char nameBuf[16];
					snprintf(nameBuf, sizeof(nameBuf), "bone%d", i);
					bone.name        = Dia::Core::StringCRC(nameBuf);
					bone.parentIndex = i > 0 ? i - 1 : -1;
					bone.localPosition = (i == 0)
					    ? Dia::Maths::Vector2D(0.0f, 0.0f)
					    : Dia::Maths::Vector2D(0.0f, boneLength);
					bone.localRotation = 0.0f;
					bone.localScale    = Dia::Maths::Vector2D(1.0f, 1.0f);
					bone.length        = boneLength;

					def->bones.Add(bone);
				}

				return def;
			}

			// Builds a straight-line limb skeleton with boneCount bones laid out along +Y.
			// Bone 0 is the root. Each subsequent bone is boneLength units along +Y from its parent.
			inline Dia::Rig2D::SkeletonDef BuildLimbSkeletonDef(int boneCount, float boneLength)
			{
				return std::move(*MakeLimbSkeletonDef(boneCount, boneLength));
			}

			// Returns an IKSolver initialized with an identity root transform.
			inline IKSolver BuildTestIKSolver(Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose)
			{
				auto solver = std::make_unique<IKSolver>(skeleton, pose);
				Dia::Rig2D::BoneTransform identity;
				solver->SetRootTransform(identity);
				return std::move(*solver);
			}
		}
	}
}
