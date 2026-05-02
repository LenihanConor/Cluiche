#include "VisualDebugger.h"

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameDataLine2D.h>
#include <DiaGraphics/Frame/DebugFrameDataCircle2D.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia
{
	namespace Rig2D
	{
		VisualDebugger::VisualDebugger(bool enabled)
			: mEnabled(enabled)
			, mShowBoneNames(false)
		{
		}

		void VisualDebugger::SetEnabled(bool enabled)
		{
			mEnabled = enabled;
		}

		bool VisualDebugger::IsEnabled() const
		{
			return mEnabled;
		}

		void VisualDebugger::SetShowBoneNames(bool show)
		{
			mShowBoneNames = show;
		}

		bool VisualDebugger::GetShowBoneNames() const
		{
			return mShowBoneNames;
		}

		void VisualDebugger::Draw(
			const Skeleton& skeleton,
			const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
			Dia::Graphics::FrameData& frameData
		)
		{
			if (!mEnabled)
				return;

			int boneCount = skeleton.GetBoneCount();

			for (int i = 0; i < boneCount; ++i)
			{
				const Bone& bone = skeleton.GetBone(i);
				const BoneTransform& wt = worldTransforms[i];

				Dia::Maths::Vector2D bonePos = wt.position;

				if (bone.parentIndex == -1)
				{
					frameData.RequestDraw(
						Dia::Graphics::DebugFrameDataCircle2D(bonePos, 4.0f, Dia::Graphics::RGBA::Green)
					);
				}
				else
				{
					const BoneTransform& parentWt = worldTransforms[bone.parentIndex];
					Dia::Maths::Vector2D parentPos = parentWt.position;

					frameData.RequestDraw(
						Dia::Graphics::DebugFrameDataLine2D(parentPos, bonePos, Dia::Graphics::RGBA::White)
					);

					frameData.RequestDraw(
						Dia::Graphics::DebugFrameDataCircle2D(bonePos, 2.5f, Dia::Graphics::RGBA::Yellow)
					);
				}
			}
		}
	}
}
