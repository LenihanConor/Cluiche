#include "PoseBlendStack.h"
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia { namespace Animation2D {

static float ShortestArcLerp(float a, float b, float t) {
    const float kPi = 3.14159265358979f;
    float diff = b - a;
    while (diff > kPi)  diff -= 2.0f * kPi;
    while (diff < -kPi) diff += 2.0f * kPi;
    return a + diff * t;
}

void PoseBlendStack::AddLayer(Dia::Core::StringCRC id,
                               const Dia::Rig2D::Pose* pose,
                               float weight,
                               int priority,
                               const BoneMask* boneMask)
{
    DIA_ASSERT(!HasLayer(id), "Duplicate layer id in PoseBlendStack");
    DIA_ASSERT(pose != nullptr, "PoseLayer pose must not be null");

    InternalLayer il;
    il.id       = id;
    il.pose     = pose;
    il.weight   = weight;
    il.priority = priority;
    il.boneMask = boneMask;

    mLayers.Add(il);
    SortByPriority();
}

void PoseBlendStack::RemoveLayer(Dia::Core::StringCRC layerId) {
    int idx = FindLayerIndex(layerId);
    if (idx == -1) return;
    for (int i = idx; i < (int)mLayers.Size() - 1; ++i)
        mLayers[i] = mLayers[i + 1];
    mLayers.Remove();
}

void PoseBlendStack::SetLayerWeight(Dia::Core::StringCRC layerId, float weight) {
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;
    int idx = FindLayerIndex(layerId);
    if (idx != -1) mLayers[idx].weight = weight;
}

void PoseBlendStack::SetLayerPriority(Dia::Core::StringCRC layerId, int priority) {
    int idx = FindLayerIndex(layerId);
    if (idx != -1) {
        mLayers[idx].priority = priority;
        SortByPriority();
    }
}

void PoseBlendStack::SetLayerBoneMask(Dia::Core::StringCRC layerId, const BoneMask* boneMask) {
    int idx = FindLayerIndex(layerId);
    if (idx != -1) mLayers[idx].boneMask = boneMask;
}

void PoseBlendStack::ResolveBoneMask(const BoneMask* boneMask,
                                      const Dia::Rig2D::Skeleton& skeleton,
                                      Dia::Core::Containers::DynamicArrayC<int, 128>& outIndices)
{
    outIndices.RemoveAll();
    if (boneMask == nullptr) return;
    for (unsigned int i = 0; i < boneMask->boneIds.Size(); ++i) {
        int idx = skeleton.FindBoneIndex(boneMask->boneIds[i]);
        if (idx == -1) continue;
        outIndices.Add(idx);
    }
}

void PoseBlendStack::Evaluate(const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& outPose) const {
    if (mLayers.Size() == 0) return;

    for (int i = 0; i < (int)mLayers.Size(); ++i) {
        DIA_ASSERT(mLayers[i].pose != &outPose, "PoseBlendStack source pose aliases outPose");
    }

    int boneCount = skeleton.GetBoneCount();

    // Scratch buffer for resolved bone indices — reused per layer
    Dia::Core::Containers::DynamicArrayC<int, 128> resolvedIndices;

    for (int i = 0; i < (int)mLayers.Size(); ++i) {
        const InternalLayer& layer = mLayers[i];
        if (layer.weight == 0.0f) continue;
        if (layer.pose == nullptr) continue;

        float w = layer.weight;

        if (layer.boneMask == nullptr) {
            // Affects all bones
            for (int b = 0; b < boneCount; ++b) {
                Dia::Rig2D::BoneTransform& out = outPose.GetLocalTransform(b);
                const Dia::Rig2D::BoneTransform& src = layer.pose->GetLocalTransform(b);
                out.position = Dia::Maths::Vector2D(
                    out.position.x + (src.position.x - out.position.x) * w,
                    out.position.y + (src.position.y - out.position.y) * w);
                out.rotation = ShortestArcLerp(out.rotation, src.rotation, w);
                out.scale = Dia::Maths::Vector2D(
                    out.scale.x + (src.scale.x - out.scale.x) * w,
                    out.scale.y + (src.scale.y - out.scale.y) * w);
            }
        } else {
            ResolveBoneMask(layer.boneMask, skeleton, resolvedIndices);
            for (int bi = 0; bi < (int)resolvedIndices.Size(); ++bi) {
                int b = resolvedIndices[bi];
                Dia::Rig2D::BoneTransform& out = outPose.GetLocalTransform(b);
                const Dia::Rig2D::BoneTransform& src = layer.pose->GetLocalTransform(b);
                out.position = Dia::Maths::Vector2D(
                    out.position.x + (src.position.x - out.position.x) * w,
                    out.position.y + (src.position.y - out.position.y) * w);
                out.rotation = ShortestArcLerp(out.rotation, src.rotation, w);
                out.scale = Dia::Maths::Vector2D(
                    out.scale.x + (src.scale.x - out.scale.x) * w,
                    out.scale.y + (src.scale.y - out.scale.y) * w);
            }
        }
    }
}

int  PoseBlendStack::GetLayerCount() const { return (int)mLayers.Size(); }
bool PoseBlendStack::HasLayer(Dia::Core::StringCRC layerId) const { return FindLayerIndex(layerId) != -1; }
void PoseBlendStack::Clear() { mLayers.RemoveAll(); }

float PoseBlendStack::GetLayerWeight(Dia::Core::StringCRC layerId) const {
    int idx = FindLayerIndex(layerId);
    return (idx != -1) ? mLayers[idx].weight : 0.0f;
}

int PoseBlendStack::GetLayerPriority(Dia::Core::StringCRC layerId) const {
    int idx = FindLayerIndex(layerId);
    return (idx != -1) ? mLayers[idx].priority : 0;
}

int PoseBlendStack::FindLayerIndex(Dia::Core::StringCRC id) const {
    for (int i = 0; i < (int)mLayers.Size(); ++i)
        if (mLayers[i].id == id) return i;
    return -1;
}

void PoseBlendStack::SortByPriority() {
    for (int i = 1; i < (int)mLayers.Size(); ++i) {
        InternalLayer key = mLayers[i];
        int j = i - 1;
        while (j >= 0 && mLayers[j].priority > key.priority) {
            mLayers[j+1] = mLayers[j];
            --j;
        }
        mLayers[j+1] = key;
    }
}

} }
