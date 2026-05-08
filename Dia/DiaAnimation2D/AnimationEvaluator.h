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

        AnimationEvaluator(const AnimationEvaluator&)            = delete;
        AnimationEvaluator& operator=(const AnimationEvaluator&) = delete;

        // Register a new AnimClipPlayer. Returns a NON-OWNING pointer; lifetime is
        // managed by this evaluator and the pointer is invalidated by UnregisterSource.
        AnimClipPlayer* RegisterClipPlayer(Dia::Core::StringCRC sourceId,
                                           int blendPriority = 0,
                                           const BoneMask* boneMask = nullptr);

        // Register a new SpringChain built from def. Returns a NON-OWNING pointer;
        // lifetime is managed by this evaluator and the pointer is invalidated by UnregisterSource.
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

        // Source iteration — index 0-based, up to GetSourceCount()-1
        int GetSourceCount() const;
        Dia::Core::StringCRC GetSourceId(int index) const;

        // Typed source access — returns nullptr if source at that ID is not the requested type
        const AnimClipPlayer* GetClipPlayer(Dia::Core::StringCRC sourceId) const;
        const SpringChain*    GetSpringChain(Dia::Core::StringCRC sourceId) const;

    private:
        enum class SourceType { kClipPlayer, kSpringChain };

        struct SourceEntry {
            Dia::Core::StringCRC id;
            SourceType type = SourceType::kClipPlayer;
            AnimClipPlayer*   clipPlayer  = nullptr; // heap-allocated, owned
            SpringChain*      springChain = nullptr; // heap-allocated, owned
            Dia::Rig2D::Pose* ownedPose   = nullptr; // heap-allocated, owned
            // autoMask removed — BoneMask is now copied into PoseBlendStack at registration
        };

        int FindSource(Dia::Core::StringCRC id) const;

        Dia::Rig2D::Skeleton& mSkeleton;
        PoseBlendStack        mBlendStack;
        Dia::Core::Containers::DynamicArrayC<SourceEntry, kMaxSources> mSources;
        Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128> mWorldTransforms;
    };
} }
