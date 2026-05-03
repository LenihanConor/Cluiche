#pragma once
#include <DiaAnimation2D/SpringChain.h>
#include <DiaAnimation2D/SpringChainDef.h>
#include <DiaRig2D/Skeleton.h>

namespace Dia { namespace Animation2D { namespace Testing {

    // Build a SpringChain covering bones from startBoneIndex to endBoneIndex (inclusive).
    // The root bone is startBoneIndex-1 (or 0 if startBoneIndex==0).
    inline SpringChain BuildTestSpringChain(
        const Dia::Rig2D::Skeleton& skeleton,
        int startBoneIndex,
        int endBoneIndex,
        float stiffness = 50.0f,
        float damping   = 5.0f,
        float maxAngularVelocity = 20.0f)
    {
        SpringChainDef def;
        def.id = Dia::Core::StringCRC("test_chain");

        int rootIdx = (startBoneIndex > 0) ? startBoneIndex - 1 : 0;
        def.rootBoneId = skeleton.GetBone(rootIdx).name;

        def.defaultNode.stiffness          = stiffness;
        def.defaultNode.damping            = damping;
        def.defaultNode.maxAngularVelocity = maxAngularVelocity;

        for (int i = startBoneIndex; i <= endBoneIndex && i < skeleton.GetBoneCount(); ++i) {
            def.boneIds.Add(skeleton.GetBone(i).name);
        }

        return SpringChain(def, skeleton);
    }

} } }
