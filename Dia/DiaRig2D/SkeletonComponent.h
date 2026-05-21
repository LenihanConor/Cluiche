#pragma once

#include "Skeleton.h"
#include "Pose.h"

namespace Dia
{
	namespace Rig2D
	{
		class SkeletonComponent
		{
		public:
			explicit SkeletonComponent(const SkeletonDef& def);
			~SkeletonComponent() = default;

			const Skeleton&		GetSkeleton() const;
			Pose&				GetCurrentPose();
			const Pose&			GetCurrentPose() const;
			void				ResetToBindPose();

		private:
			Skeleton	mSkeleton;
			Pose		mCurrentPose;
		};
	}
}
