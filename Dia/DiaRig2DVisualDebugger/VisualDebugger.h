#pragma once

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Graphics
	{
		class FrameData;
	}

	namespace Rig2D
	{
		class VisualDebugger
		{
		public:
			explicit VisualDebugger(bool enabled = true);

			void SetEnabled(bool enabled);
			bool IsEnabled() const;

			void SetShowBoneNames(bool show);
			bool GetShowBoneNames() const;

			void Draw(
				const Skeleton& skeleton,
				const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
				Dia::Graphics::FrameData& frameData
			);

		private:
			bool mEnabled;
			bool mShowBoneNames;
		};
	}
}
