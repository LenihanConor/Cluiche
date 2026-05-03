#pragma once
#include <DiaAnimation2D/SpringChainDef.h>
#include <DiaRig2D/Skeleton.h>

namespace Dia { namespace Animation2D { namespace Testing {

    inline SpringChainDef BuildTestSpringChain(
        const Dia::Core::StringCRC& id,
        const Dia::Rig2D::Skeleton& skeleton,
        int startBoneIndex, int endBoneIndex,
        float stiffness = 50.0f, float damping = 5.0f)
    {
        SpringChainDef def;
        def.id = id;
        def.rootBoneId = skeleton.GetBone(startBoneIndex > 0 ? startBoneIndex - 1 : 0).name;
        SpringNodeDef nodeParams;
        nodeParams.stiffness = stiffness;
        nodeParams.damping = damping;
        def.defaultNode = nodeParams;
        for (int i = startBoneIndex; i <= endBoneIndex; ++i) {
            def.boneIds.Add(skeleton.GetBone(i).name);
        }
        return def;
    }

} } }
