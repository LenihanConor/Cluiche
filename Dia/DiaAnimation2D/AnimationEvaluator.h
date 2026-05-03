#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaRig2D/BoneTransform.h>
#include "PoseBlendStack.h"
#include "AnimClipPlayer.h"
#include "SpringChain.h"

namespace Dia { namespace Rig2D { class Skeleton; class Pose; } }

namespace Dia { namespace Animation2D {
    class AnimationEvaluator {
    public:
        static constexpr int kMaxSources = 16;

        explicit AnimationEvaluator(Dia::Rig2D::Skeleton& skeleton);
        ~AnimationEvaluator();

        // Register a new AnimClipPlayer — returns a pointer to the owned player
        AnimClipPlayer* RegisterClipPlayer(Dia::Core::StringCRC sourceId,
                                           int blendPriority = 0,
                                           const BoneMask* boneMask = nullptr);

        // Register a new SpringChain built from def — returns a pointer to the owned chain
        SpringChain* RegisterSpringChain(Dia::Core::StringCRC sourceId,
                                          const SpringChainDef& def,
                                          int blendPriority = 0,
                                          const BoneMask* boneMask = nullptr);

        void UnregisterSource(Dia::Core::StringCRC sourceId);
        void SetSourceWeight(Dia::Core::StringCRC sourceId, float weight);
        void SetSourcePriority(Dia::Core::StringCRC sourceId, int priority);
        void SetSourceBoneMask(Dia::Core::StringCRC sourceId, const BoneMask* boneMask);

        void Evaluate(float dt,
                      const Dia::Rig2D::BoneTransform& rootTransform,
                      const Dia::Rig2D::Skeleton& skeleton,
                      Dia::Rig2D::Pose& outPose);

        PoseBlendStack&       GetBlendStack();
        const PoseBlendStack& GetBlendStack() const;

    private:
        enum class SourceType { kClipPlayer, kSpringChain };

        struct SourceEntry {
            Dia::Core::StringCRC id;
            SourceType type = SourceType::kClipPlayer;
            AnimClipPlayer* clipPlayer  = nullptr; // heap-allocated, owned
            SpringChain*    springChain = nullptr; // heap-allocated, owned
            Dia::Rig2D::Pose* ownedPose = nullptr; // heap-allocated, owned
            BoneMask        autoMask;              // auto-built for spring chains
        };

        int FindSource(Dia::Core::StringCRC id) const;

        Dia::Rig2D::Skeleton& mSkeleton;
        PoseBlendStack        mBlendStack;
        Dia::Core::Containers::DynamicArrayC<SourceEntry, kMaxSources> mSources;
        Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128> mWorldTransforms;
    };
} }
