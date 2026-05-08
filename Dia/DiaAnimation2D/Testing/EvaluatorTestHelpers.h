#pragma once
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Bone.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Animation2D { namespace Testing {

    struct TestEvaluatorFixture {
        Dia::Rig2D::Skeleton skeleton;
        AnimationEvaluator*  evaluator = nullptr;

        explicit TestEvaluatorFixture(Dia::Rig2D::SkeletonDef& def)
            : skeleton(def)
            , evaluator(new AnimationEvaluator(skeleton))
        {}

        ~TestEvaluatorFixture() {
            delete evaluator;
        }

        // Non-copyable
        TestEvaluatorFixture(const TestEvaluatorFixture&) = delete;
        TestEvaluatorFixture& operator=(const TestEvaluatorFixture&) = delete;
    };

    inline Dia::Rig2D::SkeletonDef MakeLinearSkeletonDef(int boneCount) {
        Dia::Rig2D::SkeletonDef def;
        def.id = Dia::Core::StringCRC("test_skeleton");
        const char* kNames[] = { "bone0", "bone1", "bone2", "bone3", "bone4" };
        for (int i = 0; i < boneCount && i < 5; ++i) {
            Dia::Rig2D::Bone bone;
            bone.name        = Dia::Core::StringCRC(kNames[i]);
            bone.parentIndex = i - 1;
            bone.localPosition = Dia::Maths::Vector2D(0.0f, (float)i);
            bone.length      = 1.0f;
            def.bones.Add(bone);
        }
        return def;
    }

    inline TestEvaluatorFixture* BuildTestEvaluator(int boneCount = 5) {
        static Dia::Rig2D::SkeletonDef s_def;
        s_def = MakeLinearSkeletonDef(boneCount);
        return new TestEvaluatorFixture(s_def);
    }

} } }
