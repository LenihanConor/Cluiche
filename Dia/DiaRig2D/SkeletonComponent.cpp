#include "SkeletonComponent.h"

namespace Dia
{
	namespace Rig2D
	{
		SkeletonComponent::SkeletonComponent(const SkeletonDef& def)
			: mSkeleton(def)
			, mCurrentPose(mSkeleton)
		{
		}

		const Skeleton& SkeletonComponent::GetSkeleton() const
		{
			return mSkeleton;
		}

		Pose& SkeletonComponent::GetCurrentPose()
		{
			return mCurrentPose;
		}

		const Pose& SkeletonComponent::GetCurrentPose() const
		{
			return mCurrentPose;
		}

		void SkeletonComponent::ResetToBindPose()
		{
			mCurrentPose.SetToBindPose(mSkeleton);
		}
	}
}
