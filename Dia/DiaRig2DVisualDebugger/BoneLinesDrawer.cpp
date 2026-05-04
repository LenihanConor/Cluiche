////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLinesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "BoneLinesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Dia
{
    namespace Rig2D
    {
        BoneLinesDrawer::BoneLinesDrawer(
            const Skeleton& skeleton,
            const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
            const Dia::Debug::DebugLayerManager& manager)
            : mSkeleton(skeleton)
            , mWorldTransforms(worldTransforms)
            , mManager(manager)
        {
        }

        Dia::Core::StringCRC BoneLinesDrawer::GetLayerName() const
        {
            return Dia::Debug::LayerNames::kRigBones;
        }

        void BoneLinesDrawer::Draw(Dia::Graphics::FrameData& frameData)
        {
            const int boneCount = mSkeleton.GetBoneCount();
            for (int i = 0; i < boneCount; ++i)
            {
                const Bone& bone = mSkeleton.GetBone(i);
                if (bone.parentIndex < 0)
                    continue;

                const BoneTransform& boneWt   = mWorldTransforms[i];
                const BoneTransform& parentWt = mWorldTransforms[bone.parentIndex];

                frameData.RequestDraw(
                    parentWt.position,
                    boneWt.position,
                    Dia::Debug::DebugColourPalette::kActive);
            }
        }

    } // namespace Rig2D
} // namespace Dia

#endif // DIA_DEBUG
