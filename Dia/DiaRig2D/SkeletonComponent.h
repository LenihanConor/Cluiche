#pragma once

#include <DiaCore/Architecture/Components/Interface/IComponent.h>
#include "Skeleton.h"
#include "Pose.h"

namespace Dia
{
	namespace Rig2D
	{
		class SkeletonComponent : public Dia::Core::IComponent
		{
		public:
			COMPONENT_DECLARATION(0x52494730)

			explicit SkeletonComponent(const SkeletonDef& def);
			virtual ~SkeletonComponent() {}

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
