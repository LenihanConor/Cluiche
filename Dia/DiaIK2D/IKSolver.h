#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include "IKChainDef.h"

namespace Dia
{
	namespace IK2D
	{
		class IKSolver
		{
		public:
			explicit IKSolver(Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose);

			// Must be called once per frame before any Solve* calls.
			// Stores the root transform and re-runs FK to refresh world-space bone positions.
			void SetRootTransform(const Dia::Rig2D::BoneTransform& rootTransform);

			// Chain registration — resolves bone IDs to indices at registration time.
			// DIA_ASSERT if startBoneId or endBoneId not found in skeleton.
			void RegisterChain(const IKChainDef& def);
			void UnregisterChain(Dia::Core::StringCRC chainId);
			bool HasChain(Dia::Core::StringCRC chainId) const;

			// Analytic two-bone solver. Chain must have exactly 2 joints (3 bones).
			// poleVector may be nullptr (falls back to current bone orientation).
			// Returns false if chain not found or bone count != 2 joints.
			bool SolveTwoBone(Dia::Core::StringCRC chainId,
			                  const Dia::Maths::Vector2D& target,
			                  const PoleVector* poleVector = nullptr);

			// Iterative FABRIK solver. Chain may have any number of joints (>= 1).
			// Returns false if chain not found. Returns true even if not converged
			// (best-effort result with DIA_LOG_WARNING on non-convergence).
			bool SolveFABRIK(Dia::Core::StringCRC chainId,
			                 const Dia::Maths::Vector2D& target);

			// Single-bone look-at. Rotates boneId so its forward axis points at target.
			// axisAngleOffset: correction if the bone's forward is not +X (radians).
			// Returns false if boneId not found in skeleton.
			bool SolveLookAt(Dia::Core::StringCRC boneId,
			                 const Dia::Maths::Vector2D& target,
			                 float weight          = 1.0f,
			                 float axisAngleOffset = 0.0f);
			bool SolveLookAt(int boneIndex,
			                 const Dia::Maths::Vector2D& target,
			                 float weight          = 1.0f,
			                 float axisAngleOffset = 0.0f);

			const Dia::Rig2D::Skeleton& GetSkeleton() const;
			const Dia::Rig2D::Pose&     GetPose() const;

			// ── Debugger-facing read-only accessors ──────────────────────────
			// Number of registered IK chains.
			int GetChainCount() const;

			// Chain identity and span — index is 0-based up to GetChainCount()-1.
			Dia::Core::StringCRC GetChainId(int chainIndex) const;
			int GetChainStartBoneIndex(int chainIndex) const;
			int GetChainEndBoneIndex(int chainIndex) const;
			int GetChainJointCount(int chainIndex) const;

			// Cached world-space transforms — refreshed by SetRootTransform() each frame.
			// Size matches skeleton.GetBoneCount().
			// PRECONDITION: SetRootTransform() must be called before this accessor.
			const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones>&
			    GetWorldTransforms() const;

		private:
			struct ResolvedChain
			{
				IKChainDef def;
				int        startIndex;
				int        endIndex;
				int        jointCount;
			};

			int   FindChainIndex(Dia::Core::StringCRC chainId) const;
			float GetParentWorldRot(int boneIdx) const;
			void  RefreshWorldTransforms();

			Dia::Rig2D::Skeleton& mSkeleton;
			Dia::Rig2D::Pose&     mPose;
			Dia::Rig2D::BoneTransform mRootTransform;
			bool mRootTransformSet;

			Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> mWorldTransforms;
			Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, Dia::Rig2D::kMaxBones>      mWorkingPositions;
			Dia::Core::Containers::DynamicArrayC<float, Dia::Rig2D::kMaxBones>                     mSnapshotRotations;

			static const unsigned int kMaxChains = 32;
			Dia::Core::Containers::DynamicArrayC<ResolvedChain, kMaxChains> mChains;
		};
	}
}
