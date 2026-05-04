#include "AnimationEvaluator.h"
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Core/Assert.h>
#include <new>

namespace Dia { namespace Animation2D {

static Dia::Rig2D::Pose* AllocatePose(const Dia::Rig2D::Skeleton& skeleton) {
    void* mem = ::operator new(sizeof(Dia::Rig2D::Pose));
    return new(mem) Dia::Rig2D::Pose(skeleton);
}

static void DeallocatePose(Dia::Rig2D::Pose* p) {
    if (p) { p->~Pose(); ::operator delete(p); }
}

static AnimClipPlayer* AllocateClipPlayer() {
    return new AnimClipPlayer();
}

static void DeallocateClipPlayer(AnimClipPlayer* p) {
    delete p;
}

static SpringChain* AllocateSpringChain(const SpringChainDef& def, const Dia::Rig2D::Skeleton& skeleton) {
    return new SpringChain(def, skeleton);
}

static void DeallocateSpringChain(SpringChain* p) {
    delete p;
}

AnimationEvaluator::AnimationEvaluator(Dia::Rig2D::Skeleton& skeleton)
    : mSkeleton(skeleton)
{}

AnimationEvaluator::~AnimationEvaluator() {
    for (int i = 0; i < (int)mSources.Size(); ++i) {
        DeallocatePose(mSources[i].ownedPose);
        DeallocateClipPlayer(mSources[i].clipPlayer);
        DeallocateSpringChain(mSources[i].springChain);
    }
}

AnimClipPlayer* AnimationEvaluator::RegisterClipPlayer(
    Dia::Core::StringCRC sourceId, int blendPriority, const BoneMask* boneMask)
{
    DIA_ASSERT(FindSource(sourceId) == -1, "Source id already registered");
    DIA_ASSERT((int)mSources.Size() < kMaxSources, "Max sources reached");

    SourceEntry entry;
    entry.id          = sourceId;
    entry.type        = SourceType::kClipPlayer;
    entry.clipPlayer  = AllocateClipPlayer();
    entry.springChain = nullptr;
    entry.ownedPose   = AllocatePose(mSkeleton);
    mSources.Add(entry);

    mBlendStack.AddLayer(sourceId,
                          mSources[mSources.Size()-1].ownedPose,
                          1.0f,
                          blendPriority,
                          boneMask);

    return mSources[mSources.Size()-1].clipPlayer;
}

SpringChain* AnimationEvaluator::RegisterSpringChain(
    Dia::Core::StringCRC sourceId, const SpringChainDef& def,
    int blendPriority, const BoneMask* boneMask)
{
    DIA_ASSERT(FindSource(sourceId) == -1, "Source id already registered");
    DIA_ASSERT((int)mSources.Size() < kMaxSources, "Max sources reached");

    SourceEntry entry;
    entry.id          = sourceId;
    entry.type        = SourceType::kSpringChain;
    entry.clipPlayer  = nullptr;
    entry.springChain = AllocateSpringChain(def, mSkeleton);
    entry.ownedPose   = AllocatePose(mSkeleton);

    mSources.Add(entry);

    // If no explicit bone mask is provided, auto-build one from the chain's boneIds
    // so the spring layer only overwrites the bones it controls, not the whole pose.
    // The mask is copied into PoseBlendStack, so a local is fine here.
    BoneMask autoMask;
    const BoneMask* effectiveMask = boneMask;
    if (boneMask == nullptr) {
        for (unsigned int k = 0; k < def.boneIds.Size(); ++k)
            autoMask.boneIds.Add(def.boneIds[k]);
        effectiveMask = &autoMask;
    }

    mBlendStack.AddLayer(sourceId,
                          mSources[mSources.Size()-1].ownedPose,
                          1.0f,
                          blendPriority,
                          effectiveMask);

    return mSources[mSources.Size()-1].springChain;
}

void AnimationEvaluator::UnregisterSource(Dia::Core::StringCRC sourceId) {
    int idx = FindSource(sourceId);
    if (idx == -1) return;

    mBlendStack.RemoveLayer(sourceId);
    DeallocatePose(mSources[idx].ownedPose);
    DeallocateClipPlayer(mSources[idx].clipPlayer);
    DeallocateSpringChain(mSources[idx].springChain);

    for (int i = idx; i < (int)mSources.Size() - 1; ++i)
        mSources[i] = mSources[i+1];
    mSources.Remove();
}

void AnimationEvaluator::SetSourceWeight(Dia::Core::StringCRC sourceId, float weight) {
    mBlendStack.SetLayerWeight(sourceId, weight);
}

void AnimationEvaluator::SetSourcePriority(Dia::Core::StringCRC sourceId, int priority) {
    mBlendStack.SetLayerPriority(sourceId, priority);
}

void AnimationEvaluator::SetSourceBoneMask(Dia::Core::StringCRC sourceId, const BoneMask* boneMask) {
    mBlendStack.SetLayerBoneMask(sourceId, boneMask);
}

void AnimationEvaluator::Evaluate(float dt,
                                   const Dia::Rig2D::BoneTransform& rootTransform,
                                   const Dia::Rig2D::Skeleton& skeleton,
                                   Dia::Rig2D::Pose& outPose)
{
    if (mSources.Size() == 0) return;

    outPose.SetToBindPose(skeleton);
    outPose.ComputeWorldTransforms(skeleton, rootTransform, mWorldTransforms);

    for (int i = 0; i < (int)mSources.Size(); ++i) {
        if (mSources[i].type != SourceType::kClipPlayer) continue;
        mSources[i].ownedPose->SetToBindPose(skeleton);
        mSources[i].clipPlayer->Update(dt);
        mSources[i].clipPlayer->Sample(skeleton, *mSources[i].ownedPose);
    }

    for (int i = 0; i < (int)mSources.Size(); ++i) {
        if (mSources[i].type != SourceType::kSpringChain) continue;
        // Spring chain ownedPose is NOT reset to bind each frame — it persists
        // state so the spring can accumulate displacement over time.
        mSources[i].springChain->Update(dt, *mSources[i].ownedPose, mWorldTransforms);
    }

    outPose.SetToBindPose(skeleton);
    mBlendStack.Evaluate(skeleton, outPose);
}

PoseBlendStack&       AnimationEvaluator::GetBlendStack()       { return mBlendStack; }
const PoseBlendStack& AnimationEvaluator::GetBlendStack() const { return mBlendStack; }

int AnimationEvaluator::GetSourceCount() const {
    return (int)mSources.Size();
}

Dia::Core::StringCRC AnimationEvaluator::GetSourceId(int index) const {
    DIA_ASSERT(index >= 0 && index < (int)mSources.Size(), "GetSourceId: index out of range");
    return mSources[index].id;
}

const AnimClipPlayer* AnimationEvaluator::GetClipPlayer(Dia::Core::StringCRC sourceId) const {
    int idx = FindSource(sourceId);
    if (idx == -1) return nullptr;
    if (mSources[idx].type != SourceType::kClipPlayer) return nullptr;
    return mSources[idx].clipPlayer;
}

const SpringChain* AnimationEvaluator::GetSpringChain(Dia::Core::StringCRC sourceId) const {
    int idx = FindSource(sourceId);
    if (idx == -1) return nullptr;
    if (mSources[idx].type != SourceType::kSpringChain) return nullptr;
    return mSources[idx].springChain;
}

int AnimationEvaluator::FindSource(Dia::Core::StringCRC id) const {
    for (int i = 0; i < (int)mSources.Size(); ++i)
        if (mSources[i].id == id) return i;
    return -1;
}

} }
