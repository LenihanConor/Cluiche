////////////////////////////////////////////////////////////////////////////////
// Filename: RigRestPoseRenderer.h
// Description: Fixed-layer renderer for a Skeleton's rest (bind) pose.
//              Builds once at registration; replays per frame via DebugFrameDataVisitor.
//              Emits one Line2D per non-root bone in kInactive (grey).
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Rig2D
    {
        ////////////////////////////////////////////////////////////////////////////////
        // RigRestPoseRenderer
        //
        // TypedObjectRenderer<Skeleton> — passes the Skeleton as sourceObject to
        // DebugLayerManager::RegisterFixed(). DoBuild() replicates the logic from
        // RestPoseDrawer::Draw() but writes into an IFixedPrimitiveBuffer instead
        // of FrameData. The buffer is built once and replayed every frame.
        //
        // Usage:
        //   RigRestPoseRenderer renderer;
        //   manager.RegisterFixed(kRigRestPose, &skeleton, &renderer, capacity, priority);
        ////////////////////////////////////////////////////////////////////////////////
        class RigRestPoseRenderer : public Dia::Debug::TypedObjectRenderer<Skeleton>
        {
        protected:
            void DoBuild(const Skeleton& skeleton,
                         Dia::Debug::IFixedPrimitiveBuffer& out) const override
            {
                Pose bindPose(skeleton);
                bindPose.SetToBindPose(skeleton);

                const BoneTransform identityRoot{
                    Dia::Maths::Vector2D(0.0f, 0.0f),
                    0.0f,
                    Dia::Maths::Vector2D(1.0f, 1.0f)
                };

                Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> restTransforms;
                bindPose.ComputeWorldTransforms(skeleton, identityRoot, restTransforms);

                const int boneCount = skeleton.GetBoneCount();
                for (int i = 0; i < boneCount && !out.IsFull(); ++i)
                {
                    const Bone& bone = skeleton.GetBone(i);
                    if (bone.parentIndex < 0)
                        continue;

                    out.RequestDraw(
                        restTransforms[bone.parentIndex].position,
                        restTransforms[i].position,
                        Dia::Debug::DebugColourPalette::kInactive);
                }
            }
        };

    } // namespace Rig2D
} // namespace Dia

#endif // DIA_DEBUG
