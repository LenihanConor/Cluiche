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
        void  Clear();

    private:
        struct InternalLayer {
            Dia::Core::StringCRC id;
            const Dia::Rig2D::Pose* pose = nullptr;
            float weight = 1.0f;
            int priority = 0;
            const BoneMask* boneMask = nullptr;
        };

        static const unsigned int kMaxLayers = 32;
        Dia::Core::Containers::DynamicArrayC<InternalLayer, kMaxLayers> mLayers;

        int FindLayerIndex(Dia::Core::StringCRC id) const;
        void SortByPriority();

        // Resolve bone mask to indices at evaluate time
        static void ResolveBoneMask(const BoneMask* boneMask,
                                    const Dia::Rig2D::Skeleton& skeleton,
                                    Dia::Core::Containers::DynamicArrayC<int, 128>& outIndices);
    };
} }
