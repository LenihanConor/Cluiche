#include "Skeleton.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Rig2D
	{
		Skeleton::Skeleton(const SkeletonDef& def)
			: mId(def.id)
		{
			Validate(def);

			for (unsigned int i = 0; i < def.bones.Size(); ++i)
			{
				mBones.Add(def.bones[i]);
			}

			ComputeBoneLengths();
		}

		int Skeleton::GetBoneCount() const
		{
			return static_cast<int>(mBones.Size());
		}

		const Bone& Skeleton::GetBone(int index) const
		{
			DIA_ASSERT(index >= 0 && index < static_cast<int>(mBones.Size()), "Bone index %d out of range [0, %d)", index, static_cast<int>(mBones.Size()));
			return mBones[index];
		}

		int Skeleton::FindBoneIndex(const Dia::Core::StringCRC& name) const
		{
			for (unsigned int i = 0; i < mBones.Size(); ++i)
			{
				if (mBones[i].name == name)
				{
					return static_cast<int>(i);
				}
			}

			DIA_LOG_DEBUG("Rig2D", "FindBoneIndex: bone '%s' not found in skeleton '%s'", name.AsChar(), mId.AsChar());
			return -1;
		}

		int Skeleton::GetRequiredBoneIndex(const Dia::Core::StringCRC& name) const
		{
			int index = FindBoneIndex(name);
			if (index < 0)
			{
				DIA_LOG_WARNING("Rig2D", "GetRequiredBoneIndex: bone '%s' not found in skeleton '%s'", name.AsChar(), mId.AsChar());
				DIA_ASSERT(false, "Required bone '%s' not found in skeleton '%s'", name.AsChar(), mId.AsChar());
			}
			return index;
		}

		const Dia::Core::StringCRC& Skeleton::GetId() const
		{
			return mId;
		}

		bool Skeleton::IsValid() const
		{
			if (mBones.Size() == 0)
				return false;

			int rootCount = 0;
			for (unsigned int i = 0; i < mBones.Size(); ++i)
			{
				int parentIdx = mBones[i].parentIndex;

				if (parentIdx == -1)
				{
					rootCount++;
				}
				else if (parentIdx < 0 || parentIdx >= static_cast<int>(i))
				{
					return false;
				}
			}

			return rootCount == 1;
		}

		void Skeleton::Validate(const SkeletonDef& def) const
		{
			DIA_ASSERT(def.bones.Size() > 0, "SkeletonDef must have at least one bone");
			DIA_ASSERT(def.bones.Size() <= kMaxBones, "SkeletonDef has %u bones, max is %u", def.bones.Size(), kMaxBones);

			int rootCount = 0;
			for (unsigned int i = 0; i < def.bones.Size(); ++i)
			{
				int parentIdx = def.bones[i].parentIndex;

				if (parentIdx == -1)
				{
					rootCount++;
				}
				else
				{
					DIA_ASSERT(parentIdx >= 0 && parentIdx < static_cast<int>(i),
						"Bone %u ('%s') has parent index %d which violates topological ordering",
						i, def.bones[i].name.AsChar(), parentIdx);
				}

				for (unsigned int j = 0; j < i; ++j)
				{
					DIA_ASSERT(def.bones[i].name != def.bones[j].name,
						"Duplicate bone name '%s' at indices %u and %u",
						def.bones[i].name.AsChar(), j, i);
				}
			}

			DIA_ASSERT(rootCount == 1, "Skeleton must have exactly one root bone, found %d", rootCount);
		}

		void Skeleton::ComputeBoneLengths()
		{
			for (unsigned int i = 0; i < mBones.Size(); ++i)
			{
				if (mBones[i].parentIndex == -1)
				{
					mBones[i].length = 0.0f;
				}
				else if (mBones[i].length == 0.0f)
				{
					mBones[i].length = mBones[i].localPosition.Magnitude();
				}
			}
		}
	}
}
