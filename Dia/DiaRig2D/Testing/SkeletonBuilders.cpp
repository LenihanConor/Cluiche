#include "SkeletonBuilders.h"

namespace Dia
{
	namespace Rig2D
	{
		namespace Testing
		{
			SkeletonDef MakeSimpleChain(int boneCount)
			{
				SkeletonDef def;
				def.id = Dia::Core::StringCRC("simple_chain");

				for (int i = 0; i < boneCount; ++i)
				{
					Bone bone;

					char name[32];
					snprintf(name, sizeof(name), "bone_%d", i);
					bone.name = Dia::Core::StringCRC(name);

					if (i == 0)
					{
						bone.parentIndex = -1;
						bone.localPosition.Set(0.0f, 0.0f);
					}
					else
					{
						bone.parentIndex = i - 1;
						bone.localPosition.Set(0.0f, 1.0f);
					}

					bone.localRotation = 0.0f;
					bone.localScale.Set(1.0f, 1.0f);

					def.bones.Add(bone);
				}

				return def;
			}

			SkeletonDef MakeHumanoid()
			{
				SkeletonDef def;
				def.id = Dia::Core::StringCRC("humanoid");

				auto addBone = [&](const char* name, int parentIdx, float x, float y)
				{
					Bone bone;
					bone.name = Dia::Core::StringCRC(name);
					bone.parentIndex = parentIdx;
					bone.localPosition.Set(x, y);
					bone.localRotation = 0.0f;
					bone.localScale.Set(1.0f, 1.0f);
					def.bones.Add(bone);
				};

				addBone("root",       -1, 0.0f, 0.0f);     // 0
				addBone("hips",        0, 0.0f, 1.0f);      // 1
				addBone("spine",       1, 0.0f, 1.0f);      // 2
				addBone("chest",       2, 0.0f, 1.0f);      // 3
				addBone("head",        3, 0.0f, 0.5f);      // 4
				addBone("upper_arm_l", 3, -0.5f, 0.0f);     // 5
				addBone("lower_arm_l", 5, -0.5f, 0.0f);     // 6
				addBone("upper_arm_r", 3, 0.5f, 0.0f);      // 7
				addBone("lower_arm_r", 7, 0.5f, 0.0f);      // 8
				addBone("upper_leg_l", 1, -0.3f, -1.0f);    // 9
				addBone("lower_leg_l", 9, 0.0f, -1.0f);     // 10
				addBone("upper_leg_r", 1, 0.3f, -1.0f);     // 11
				addBone("lower_leg_r", 11, 0.0f, -1.0f);    // 12

				return def;
			}

			SkeletonDef MakeBranching()
			{
				SkeletonDef def;
				def.id = Dia::Core::StringCRC("branching");

				auto addBone = [&](const char* name, int parentIdx, float x, float y)
				{
					Bone bone;
					bone.name = Dia::Core::StringCRC(name);
					bone.parentIndex = parentIdx;
					bone.localPosition.Set(x, y);
					bone.localRotation = 0.0f;
					bone.localScale.Set(1.0f, 1.0f);
					def.bones.Add(bone);
				};

				addBone("root",    -1, 0.0f, 0.0f);     // 0
				addBone("child_a",  0, -1.0f, 1.0f);     // 1
				addBone("child_b",  0, 0.0f, 1.0f);      // 2
				addBone("child_c",  0, 1.0f, 1.0f);      // 3
				addBone("grandchild_b1", 2, -0.5f, 1.0f); // 4
				addBone("grandchild_b2", 2, 0.5f, 1.0f);  // 5

				return def;
			}
		}
	}
}
