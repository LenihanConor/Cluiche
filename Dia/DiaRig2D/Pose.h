#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "BoneTransform.h"
#include "Skeleton.h"

namespace Dia
{
	namespace Rig2D
	{
		class Pose
		{
		public:
			explicit Pose(const Skeleton& skeleton);

			int					GetBoneCount() const;
			BoneTransform&		GetLocalTransform(int boneIndex);
			const BoneTransform& GetLocalTransform(int boneIndex) const;

			void SetToBindPose(const Skeleton& skeleton);

			void ComputeWorldTransforms(
				const Skeleton& skeleton,
				const BoneTransform& rootTransform,
				Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& outWorldTransforms
			) const;

		private:
			Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> mLocalTransforms;
		};
	}
}
