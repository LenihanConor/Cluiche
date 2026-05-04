#include "IKSolver.h"

#include <cmath>

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace IK2D
	{
		namespace
		{
			inline float ShortestArcLerp(float a, float b, float t)
			{
				float diff = b - a;
				diff = atan2f(sinf(diff), cosf(diff));
				return a + t * diff;
			}

			inline float Clamp(float v, float lo, float hi)
			{
				return v < lo ? lo : (v > hi ? hi : v);
			}

			// The world rotation of a bone (parent world rot P) is related to the direction
			// to the next bone by: direction_angle = world_rot + bone_rest_angle
			// where bone_rest_angle = atan2(localPos.y, localPos.x)
			// So: world_rot = direction_angle - bone_rest_angle
			inline float BoneWorldRotFromDir(float dirAngle, float boneRestAngle)
			{
				return dirAngle - boneRestAngle;
			}
		}

		IKSolver::IKSolver(Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose)
			: mSkeleton(skeleton)
			, mPose(pose)
			, mRootTransform()
			, mRootTransformSet(false)
		{
			const int boneCount = skeleton.GetBoneCount();
			for (int i = 0; i < boneCount; ++i)
			{
				mWorldTransforms.AddDefault();
				mWorkingPositions.AddDefault();
				mSnapshotRotations.Add(0.0f);
			}
		}

		void IKSolver::SetRootTransform(const Dia::Rig2D::BoneTransform& rootTransform)
		{
			mRootTransform    = rootTransform;
			mRootTransformSet = true;
			RefreshWorldTransforms();
		}

		void IKSolver::RegisterChain(const IKChainDef& def)
		{
			int startIndex = mSkeleton.FindBoneIndex(def.startBoneId);
			int endIndex   = mSkeleton.FindBoneIndex(def.endBoneId);

			DIA_ASSERT(startIndex >= 0, "IKSolver::RegisterChain — startBoneId not found in skeleton");
			DIA_ASSERT(endIndex   >= 0, "IKSolver::RegisterChain — endBoneId not found in skeleton");
			DIA_ASSERT(startIndex < endIndex, "IKSolver::RegisterChain — startIndex must be < endIndex");

			ResolvedChain chain;
			chain.def        = def;
			chain.startIndex = startIndex;
			chain.endIndex   = endIndex;
			chain.jointCount = endIndex - startIndex;
			mChains.Add(chain);
		}

		void IKSolver::UnregisterChain(Dia::Core::StringCRC chainId)
		{
			const int idx = FindChainIndex(chainId);
			if (idx >= 0)
				mChains.RemoveAt(static_cast<unsigned int>(idx));
		}

		bool IKSolver::HasChain(Dia::Core::StringCRC chainId) const
		{
			return FindChainIndex(chainId) >= 0;
		}

		const Dia::Rig2D::Skeleton& IKSolver::GetSkeleton() const { return mSkeleton; }
		const Dia::Rig2D::Pose&     IKSolver::GetPose()     const { return mPose; }

		int IKSolver::GetChainCount() const
		{
			return static_cast<int>(mChains.Size());
		}

		Dia::Core::StringCRC IKSolver::GetChainId(int chainIndex) const
		{
			DIA_ASSERT(chainIndex >= 0 && chainIndex < static_cast<int>(mChains.Size()),
			           "IKSolver::GetChainId — chainIndex out of range");
			return mChains[chainIndex].def.id;
		}

		int IKSolver::GetChainStartBoneIndex(int chainIndex) const
		{
			DIA_ASSERT(chainIndex >= 0 && chainIndex < static_cast<int>(mChains.Size()),
			           "IKSolver::GetChainStartBoneIndex — chainIndex out of range");
			return mChains[chainIndex].startIndex;
		}

		int IKSolver::GetChainEndBoneIndex(int chainIndex) const
		{
			DIA_ASSERT(chainIndex >= 0 && chainIndex < static_cast<int>(mChains.Size()),
			           "IKSolver::GetChainEndBoneIndex — chainIndex out of range");
			return mChains[chainIndex].endIndex;
		}

		int IKSolver::GetChainJointCount(int chainIndex) const
		{
			DIA_ASSERT(chainIndex >= 0 && chainIndex < static_cast<int>(mChains.Size()),
			           "IKSolver::GetChainJointCount — chainIndex out of range");
			return mChains[chainIndex].jointCount;
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones>&
		    IKSolver::GetWorldTransforms() const
		{
			DIA_ASSERT(mRootTransformSet,
			           "IKSolver::GetWorldTransforms — SetRootTransform() must be called before drawing");
			return mWorldTransforms;
		}

		int IKSolver::FindChainIndex(Dia::Core::StringCRC chainId) const
		{
			for (unsigned int i = 0; i < mChains.Size(); ++i)
			{
				if (mChains[i].def.id == chainId)
					return static_cast<int>(i);
			}
			return -1;
		}

		void IKSolver::RefreshWorldTransforms()
		{
			mPose.ComputeWorldTransforms(mSkeleton, mRootTransform, mWorldTransforms);
		}

		// Returns the world rotation of bone[startIdx]'s parent.
		float IKSolver::GetParentWorldRot(int boneIdx) const
		{
			const int parentIdx = mSkeleton.GetBone(boneIdx).parentIndex;
			return (parentIdx >= 0) ? mWorldTransforms[parentIdx].rotation : mRootTransform.rotation;
		}

		// ─── Two-Bone Solver ────────────────────────────────────────────────────────

		bool IKSolver::SolveTwoBone(Dia::Core::StringCRC chainId,
		                            const Dia::Maths::Vector2D& target,
		                            const PoleVector* poleVector)
		{
			DIA_ASSERT(mRootTransformSet, "IKSolver::SolveTwoBone — SetRootTransform must be called before Solve*");

			const int chainIdx = FindChainIndex(chainId);
			if (chainIdx < 0)
				return false;

			const ResolvedChain& chain = mChains[chainIdx];

			if (chain.jointCount != 2)
			{
				DIA_ASSERT(false, "IKSolver::SolveTwoBone — chain must have exactly 2 joints");
				return false;
			}

			const int startIdx = chain.startIndex;
			const int midIdx   = startIdx + 1;
			const int endIdx   = chain.endIndex;

			// Snapshot rotations for reach weight
			mSnapshotRotations[startIdx] = mPose.GetLocalTransform(startIdx).rotation;
			mSnapshotRotations[midIdx]   = mPose.GetLocalTransform(midIdx).rotation;
			mSnapshotRotations[endIdx]   = mPose.GetLocalTransform(endIdx).rotation;

			const Dia::Maths::Vector2D& startPos = mWorldTransforms[startIdx].position;

			// l1 = length of the segment from start->mid (stored on mid bone)
			// l2 = length of the segment from mid->end (stored on end bone)
			const float l1 = mSkeleton.GetBone(midIdx).length;
			const float l2 = mSkeleton.GetBone(endIdx).length;

			const Dia::Maths::Vector2D toTarget = target - startPos;
			const float d = toTarget.Magnitude();

			// bone rest angles: atan2(localPos.y, localPos.x) for each segment
			const float restMid = atan2f(mSkeleton.GetBone(midIdx).localPosition.Y(),
			                             mSkeleton.GetBone(midIdx).localPosition.X());
			const float restEnd = atan2f(mSkeleton.GetBone(endIdx).localPosition.Y(),
			                             mSkeleton.GetBone(endIdx).localPosition.X());

			float startWorldAngle;
			float midWorldAngle;

			if (d >= l1 + l2)
			{
				// Unreachable: extend fully toward target
				DIA_LOG_WARNING("Rig2D", "IKSolver::SolveTwoBone — target unreachable, extending chain");
				const float dirAngle = atan2f(toTarget.y, toTarget.x);
				startWorldAngle = BoneWorldRotFromDir(dirAngle, restMid);
				midWorldAngle   = startWorldAngle; // straight extension: same world rotation
			}
			else
			{
				// Law of cosines for angle at start (in the triangle start-elbow-target)
				const float cosStartOffset = Clamp((l1 * l1 + d * d - l2 * l2) / (2.0f * l1 * d), -1.0f, 1.0f);
				const float startOffset    = acosf(cosStartOffset);

				// Direction from start to target
				const float alpha = atan2f(toTarget.y, toTarget.x);

				// Law of cosines for mid-joint interior angle
				const float cosAngle = Clamp((l1 * l1 + l2 * l2 - d * d) / (2.0f * l1 * l2), -1.0f, 1.0f);
				const float absAngle = acosf(cosAngle);
				const float turnAngle = Dia::Maths::PI - absAngle; // always >= 0

				// bendCCW=true → elbow at (alpha - startOffset), turn at elbow is CCW (+turnAngle).
				// bendCCW=false → elbow at (alpha + startOffset), turn at elbow is CW (-turnAngle).
				bool bendCCW = true;
				if (poleVector != nullptr && poleVector->weight > 0.0f)
				{
					const float elbowDirA = alpha - startOffset; // CCW option
					const float elbowDirB = alpha + startOffset; // CW option
					const float dotA = cosf(elbowDirA) * poleVector->direction.x + sinf(elbowDirA) * poleVector->direction.y;
					const float dotB = cosf(elbowDirB) * poleVector->direction.x + sinf(elbowDirB) * poleVector->direction.y;
					bendCCW = (dotA >= dotB);
				}

				const float elbowDirAngle = bendCCW ? (alpha - startOffset) : (alpha + startOffset);
				startWorldAngle = BoneWorldRotFromDir(elbowDirAngle, restMid);

				const float midDirAngle = bendCCW
				    ? (elbowDirAngle + turnAngle)
				    : (elbowDirAngle - turnAngle);
				midWorldAngle = BoneWorldRotFromDir(midDirAngle, restEnd);
			}

			const float parentAngle = GetParentWorldRot(startIdx);

			// Apply joint limits
			float startLocalAngle = startWorldAngle - parentAngle;
			if (!chain.def.jointLimits.IsEmpty() && chain.def.jointLimits[0].enabled)
			{
				startLocalAngle = Clamp(startLocalAngle,
				                        chain.def.jointLimits[0].minAngle,
				                        chain.def.jointLimits[0].maxAngle);
			}

			float midLocalAngle = midWorldAngle - startWorldAngle;
			if (chain.def.jointLimits.Size() >= 2 && chain.def.jointLimits[1].enabled)
			{
				midLocalAngle = Clamp(midLocalAngle,
				                      chain.def.jointLimits[1].minAngle,
				                      chain.def.jointLimits[1].maxAngle);
			}

			const float reachWeight = Clamp(chain.def.reachWeight, 0.0f, 1.0f);

			mPose.GetLocalTransform(startIdx).rotation =
			    ShortestArcLerp(mSnapshotRotations[startIdx], startLocalAngle, reachWeight);
			mPose.GetLocalTransform(midIdx).rotation =
			    ShortestArcLerp(mSnapshotRotations[midIdx], midLocalAngle, reachWeight);

			RefreshWorldTransforms();
			return true;
		}

		// ─── FABRIK Solver ──────────────────────────────────────────────────────────

		bool IKSolver::SolveFABRIK(Dia::Core::StringCRC chainId,
		                           const Dia::Maths::Vector2D& target)
		{
			DIA_ASSERT(mRootTransformSet, "IKSolver::SolveFABRIK — SetRootTransform must be called before Solve*");

			const int chainIdx = FindChainIndex(chainId);
			if (chainIdx < 0)
				return false;

			const ResolvedChain& chain = mChains[chainIdx];
			const int startIdx = chain.startIndex;
			const int endIdx   = chain.endIndex;

			// Snapshot rotations for reach weight
			for (int i = startIdx; i <= endIdx; ++i)
				mSnapshotRotations[i] = mPose.GetLocalTransform(i).rotation;

			// Total chain length
			float totalLength = 0.0f;
			for (int i = startIdx + 1; i <= endIdx; ++i)
				totalLength += mSkeleton.GetBone(i).length;

			const Dia::Maths::Vector2D startWorldPos = mWorldTransforms[startIdx].position;
			const Dia::Maths::Vector2D toTarget = target - startWorldPos;
			const float distToTarget = toTarget.Magnitude();

			// Copy world positions into working array
			for (int i = startIdx; i <= endIdx; ++i)
				mWorkingPositions[i] = mWorldTransforms[i].position;

			if (distToTarget >= totalLength)
			{
				// Unreachable: extend fully toward target
				DIA_LOG_WARNING("Rig2D", "IKSolver::SolveFABRIK — target unreachable, extending chain");
				const Dia::Maths::Vector2D dir = toTarget.AsNormal();
				float accumulated = 0.0f;
				mWorkingPositions[startIdx] = startWorldPos;
				for (int i = startIdx + 1; i <= endIdx; ++i)
				{
					// Each bone contributes its segment length along the direction
					// Compute the direction offset accounting for bone rest angle
					const float boneLen      = mSkeleton.GetBone(i).length;
					const float boneRestAngle = atan2f(mSkeleton.GetBone(i).localPosition.Y(),
					                                    mSkeleton.GetBone(i).localPosition.X());
					// World rotation needed to point this bone toward dir
					const float dirAngle  = atan2f(dir.y, dir.x);
					const float worldRot  = dirAngle - boneRestAngle;
					const float dirActual = worldRot + boneRestAngle; // = dirAngle
					accumulated += boneLen;
					mWorkingPositions[i] = startWorldPos + dir * accumulated;
					(void)worldRot; (void)dirActual; // used implicitly via dir
				}
			}
			else
			{
				bool converged = false;
				for (int iter = 0; iter < chain.def.maxIterations; ++iter)
				{
					// Backward pass: pin end effector at target
					mWorkingPositions[endIdx] = target;
					for (int i = endIdx - 1; i >= startIdx; --i)
					{
						const float boneLen = mSkeleton.GetBone(i + 1).length;
						Dia::Maths::Vector2D dir = mWorkingPositions[i] - mWorkingPositions[i + 1];
						const float mag = dir.Magnitude();
						if (mag > 1e-6f)
							dir = dir * (boneLen / mag);
						else
							dir = Dia::Maths::Vector2D(boneLen, 0.0f);
						mWorkingPositions[i] = mWorkingPositions[i + 1] + dir;
					}

					// Forward pass: pin root
					mWorkingPositions[startIdx] = startWorldPos;
					for (int i = startIdx + 1; i <= endIdx; ++i)
					{
						const float boneLen = mSkeleton.GetBone(i).length;
						Dia::Maths::Vector2D dir = mWorkingPositions[i] - mWorkingPositions[i - 1];
						const float mag = dir.Magnitude();
						if (mag > 1e-6f)
							dir = dir * (boneLen / mag);
						else
							dir = Dia::Maths::Vector2D(boneLen, 0.0f);
						mWorkingPositions[i] = mWorkingPositions[i - 1] + dir;
					}

					// Apply joint limits per iteration
					float parentWorldRot = GetParentWorldRot(startIdx);
					for (int i = startIdx; i < endIdx; ++i)
					{
						const Dia::Rig2D::Bone& nextBone  = mSkeleton.GetBone(i + 1);
						const float boneRestAngle = atan2f(nextBone.localPosition.Y(), nextBone.localPosition.X());
						Dia::Maths::Vector2D boneDir = mWorkingPositions[i + 1] - mWorkingPositions[i];
						const float dirAngle  = atan2f(boneDir.y, boneDir.x);
						float worldRot  = BoneWorldRotFromDir(dirAngle, boneRestAngle);

						const int limitIdx = i - startIdx;
						if (limitIdx < static_cast<int>(chain.def.jointLimits.Size()) &&
						    chain.def.jointLimits[limitIdx].enabled)
						{
							float localRot = worldRot - parentWorldRot;
							localRot = Clamp(localRot,
							                 chain.def.jointLimits[limitIdx].minAngle,
							                 chain.def.jointLimits[limitIdx].maxAngle);
							worldRot = localRot + parentWorldRot;
							const float newDirAngle = worldRot + boneRestAngle;
							const float boneLen = nextBone.length;
							mWorkingPositions[i + 1] = mWorkingPositions[i] +
							    Dia::Maths::Vector2D(cosf(newDirAngle) * boneLen, sinf(newDirAngle) * boneLen);
						}

						parentWorldRot = worldRot;
					}

					// Convergence check
					const float dist = (mWorkingPositions[endIdx] - target).Magnitude();
					if (dist < chain.def.tolerance)
					{
						converged = true;
						break;
					}
				}

				if (!converged)
				{
					DIA_LOG_WARNING("Rig2D", "IKSolver::SolveFABRIK — did not converge within maxIterations");
				}
			}

			// Convert working world positions back to local rotations
			const float reachWeight = Clamp(chain.def.reachWeight, 0.0f, 1.0f);
			float parentWorldRot = GetParentWorldRot(startIdx);

			for (int i = startIdx; i < endIdx; ++i)
			{
				const Dia::Rig2D::Bone& nextBone  = mSkeleton.GetBone(i + 1);
				const float boneRestAngle = atan2f(nextBone.localPosition.Y(), nextBone.localPosition.X());
				Dia::Maths::Vector2D boneDir = mWorkingPositions[i + 1] - mWorkingPositions[i];
				const float dirAngle  = atan2f(boneDir.y, boneDir.x);
				const float worldRot  = BoneWorldRotFromDir(dirAngle, boneRestAngle);
				const float localRot  = worldRot - parentWorldRot;
				const float blended   = ShortestArcLerp(mSnapshotRotations[i], localRot, reachWeight);
				mPose.GetLocalTransform(i).rotation = blended;
				parentWorldRot = worldRot;
			}

			RefreshWorldTransforms();
			return true;
		}

		// ─── Look-At Constraint ─────────────────────────────────────────────────────

		bool IKSolver::SolveLookAt(Dia::Core::StringCRC boneId,
		                           const Dia::Maths::Vector2D& target,
		                           float weight,
		                           float axisAngleOffset)
		{
			const int idx = mSkeleton.FindBoneIndex(boneId);
			if (idx < 0)
				return false;
			return SolveLookAt(idx, target, weight, axisAngleOffset);
		}

		bool IKSolver::SolveLookAt(int boneIndex,
		                           const Dia::Maths::Vector2D& target,
		                           float weight,
		                           float axisAngleOffset)
		{
			DIA_ASSERT(mRootTransformSet, "IKSolver::SolveLookAt — SetRootTransform must be called before Solve*");
			DIA_ASSERT(boneIndex >= 0 && boneIndex < mSkeleton.GetBoneCount(),
			           "IKSolver::SolveLookAt — boneIndex out of range");

			const float snapshotRot = mPose.GetLocalTransform(boneIndex).rotation;
			const Dia::Maths::Vector2D& boneWorldPos = mWorldTransforms[boneIndex].position;

			// Coincident guard
			const float dist = (target - boneWorldPos).Magnitude();
			if (dist < 1e-5f)
			{
				DIA_LOG_WARNING("Rig2D", "IKSolver::SolveLookAt — target coincident with bone position, skipping");
				return true;
			}

			const float worldAngle = atan2f(target.y - boneWorldPos.y,
			                                target.x - boneWorldPos.x) - axisAngleOffset;

			const float parentWorldAngle = GetParentWorldRot(boneIndex);
			const float localAngle = worldAngle - parentWorldAngle;
			const float blended    = ShortestArcLerp(snapshotRot, localAngle, Clamp(weight, 0.0f, 1.0f));
			mPose.GetLocalTransform(boneIndex).rotation = blended;

			RefreshWorldTransforms();
			return true;
		}
	}
}
