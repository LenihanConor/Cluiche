#include "Pose.h"

#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia
{
	namespace Rig2D
	{
		namespace
		{
			struct Affine2x3
			{
				float m00, m01, m02;
				float m10, m11, m12;
			};

			Affine2x3 ComposeAffine(const BoneTransform& t)
			{
				float cosR = cosf(t.rotation);
				float sinR = sinf(t.rotation);
				float sx = t.scale.X();
				float sy = t.scale.Y();

				Affine2x3 m;
				m.m00 = cosR * sx;  m.m01 = -sinR * sy; m.m02 = t.position.X();
				m.m10 = sinR * sx;  m.m11 =  cosR * sy; m.m12 = t.position.Y();
				return m;
			}

			Affine2x3 MultiplyAffine(const Affine2x3& a, const Affine2x3& b)
			{
				Affine2x3 r;
				r.m00 = a.m00 * b.m00 + a.m01 * b.m10;
				r.m01 = a.m00 * b.m01 + a.m01 * b.m11;
				r.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02;
				r.m10 = a.m10 * b.m00 + a.m11 * b.m10;
				r.m11 = a.m10 * b.m01 + a.m11 * b.m11;
				r.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12;
				return r;
			}

			BoneTransform DecomposeAffine(const Affine2x3& m)
			{
				BoneTransform t;
				t.position.Set(m.m02, m.m12);

				float sx = sqrtf(m.m00 * m.m00 + m.m10 * m.m10);
				float sy = sqrtf(m.m01 * m.m01 + m.m11 * m.m11);

				float det = m.m00 * m.m11 - m.m01 * m.m10;
				if (det < 0.0f)
				{
					sx = -sx;
				}

				t.scale.Set(sx, sy);

				if (sx != 0.0f)
				{
					t.rotation = atan2f(m.m10 / sx, m.m00 / sx);
				}
				else
				{
					t.rotation = 0.0f;
				}

				return t;
			}
		}

		Pose::Pose(const Skeleton& skeleton)
		{
			SetToBindPose(skeleton);
		}

		int Pose::GetBoneCount() const
		{
			return static_cast<int>(mLocalTransforms.Size());
		}

		BoneTransform& Pose::GetLocalTransform(int boneIndex)
		{
			DIA_ASSERT(boneIndex >= 0 && boneIndex < static_cast<int>(mLocalTransforms.Size()),
				"Bone index %d out of range [0, %d)", boneIndex, static_cast<int>(mLocalTransforms.Size()));
			return mLocalTransforms[boneIndex];
		}

		const BoneTransform& Pose::GetLocalTransform(int boneIndex) const
		{
			DIA_ASSERT(boneIndex >= 0 && boneIndex < static_cast<int>(mLocalTransforms.Size()),
				"Bone index %d out of range [0, %d)", boneIndex, static_cast<int>(mLocalTransforms.Size()));
			return mLocalTransforms[boneIndex];
		}

		void Pose::SetToBindPose(const Skeleton& skeleton)
		{
			mLocalTransforms.RemoveAll();
			for (int i = 0; i < skeleton.GetBoneCount(); ++i)
			{
				const Bone& bone = skeleton.GetBone(i);
				BoneTransform bt;
				bt.position = bone.localPosition;
				bt.rotation = bone.localRotation;
				bt.scale = bone.localScale;
				mLocalTransforms.Add(bt);
			}
		}

		void Pose::ComputeWorldTransforms(
			const Skeleton& skeleton,
			const BoneTransform& rootTransform,
			Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& outWorldTransforms
		) const
		{
			int boneCount = skeleton.GetBoneCount();
			DIA_ASSERT(static_cast<int>(mLocalTransforms.Size()) == boneCount,
				"Pose bone count %d does not match skeleton bone count %d",
				static_cast<int>(mLocalTransforms.Size()), boneCount);

			outWorldTransforms.RemoveAll();

			Affine2x3 rootMatrix = ComposeAffine(rootTransform);

			static Affine2x3 worldMatrices[kMaxBones];

			for (int i = 0; i < boneCount; ++i)
			{
				Affine2x3 localMatrix = ComposeAffine(mLocalTransforms[i]);

				int parentIdx = skeleton.GetBone(i).parentIndex;
				if (parentIdx == -1)
				{
					worldMatrices[i] = MultiplyAffine(rootMatrix, localMatrix);
				}
				else
				{
					worldMatrices[i] = MultiplyAffine(worldMatrices[parentIdx], localMatrix);
				}

				outWorldTransforms.Add(DecomposeAffine(worldMatrices[i]));
			}
		}
	}
}
