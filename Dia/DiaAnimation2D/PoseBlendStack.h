#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "BoneMask.h"

namespace Dia { namespace Rig2D { class Skeleton; class Pose; } }

namespace Dia { namespace Animation2D {
    class PoseBlendStack {
    public:
        PoseBlendStack() = default;

        void AddLayer(Dia::Core::StringCRC id,
                      const Dia::Rig2D::Pose* pose,
                      float weight,
                      int priority,
                      const BoneMask* boneMask = nullptr);

        void RemoveLayer(Dia::Core::StringCRC layerId);
        void SetLayerWeight(Dia::Core::StringCRC layerId, float weight);
        void SetLayerPriority(Dia::Core::StringCRC layerId, int priority);
        void SetLayerBoneMask(Dia::Core::StringCRC layerId, const BoneMask* boneMask);

        void Evaluate(const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& outPose) const;

        int   GetLayerCount() const;
        bool  HasLayer(Dia::Core::StringCRC layerId) const;
        float GetLayerWeight(Dia::Core::StringCRC layerId) const;
        int   GetLayerPriority(Dia::Core::StringCRC layerId) const;
        // Index-based layer ID access — enables iteration without knowing IDs in advance.
        // Combined with GetLayerCount(), GetLayerWeight(id), GetLayerPriority(id) for full iteration.
        Dia::Core::StringCRC GetLayerId(int index) const;
        void  Clear();

    private:
        // Compact bone mask stored per layer — smaller capacity than the public BoneMask
        // to keep InternalLayer size manageable inside the fixed DynamicArrayC<kMaxLayers>.
        static constexpr unsigned int kMaxBonesPerLayerMask = 32;
        struct LayerBoneMask {
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxBonesPerLayerMask> boneIds;
        };

        struct InternalLayer {
            Dia::Core::StringCRC id;
            const Dia::Rig2D::Pose* pose = nullptr;
            float weight = 1.0f;
            int priority = 0;
            LayerBoneMask boneMask;  // owned copy — resolved lazily to indices at Evaluate time
            bool          hasBoneMask = false;
        };

        static const unsigned int kMaxLayers = 32;
        Dia::Core::Containers::DynamicArrayC<InternalLayer, kMaxLayers> mLayers;

        int FindLayerIndex(Dia::Core::StringCRC id) const;
        void SortByPriority();

        // Resolve bone mask to indices at evaluate time
        static void ResolveBoneMask(const LayerBoneMask& boneMask,
                                    const Dia::Rig2D::Skeleton& skeleton,
                                    Dia::Core::Containers::DynamicArrayC<int, 128>& outIndices);
    };
} }
