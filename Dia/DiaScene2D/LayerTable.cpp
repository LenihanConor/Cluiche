////////////////////////////////////////////////////////////////////////////////
// Filename: LayerTable.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaScene2D/LayerTable.h"

namespace Dia
{
    namespace Scene2D
    {
        LayerTable::LayerTable()
            : mCount(0)
        {
        }

        void LayerTable::Build(const Dia::Core::Containers::DynamicArrayC<LayerDef, 32>& layers)
        {
            mCount = 0;

            // Check if caller explicitly provided a "default" layer
            const Dia::Core::StringCRC defaultCrc(kDefaultLayerId);
            bool hasDefault = false;
            for (unsigned int i = 0; i < layers.Size(); ++i)
            {
                if (layers.At(i).id == defaultCrc)
                {
                    hasDefault = true;
                    break;
                }
            }

            // Inject "default" at slot 0 if absent
            if (!hasDefault)
            {
                LayerDef& def      = mLayers[mCount++];
                def.id             = defaultCrc;
                def.sortOrder      = 0;
                def.parallax       = {1.0f, 1.0f};
                def.sortPolicy     = Dia::Core::StringCRC("insertion");
                def.enabled        = true;
                def.renderTechnique = Dia::Core::StringCRC();
            }

            // Add remaining layers, stop at kMaxLayers
            for (unsigned int i = 0; i < layers.Size() && mCount < kMaxLayers; ++i)
            {
                mLayers[mCount++] = layers.At(i);
            }
        }

        unsigned int LayerTable::GetBitIndex(Dia::Core::StringCRC layerId) const
        {
            int idx = FindIndex(layerId);
            return (idx >= 0) ? static_cast<unsigned int>(idx) : 0u;
        }

        uint32_t LayerTable::ResolveMask(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& layerNames) const
        {
            uint32_t mask = 0u;
            for (unsigned int i = 0; i < layerNames.Size(); ++i)
            {
                unsigned int bit = GetBitIndex(layerNames.At(i));
                mask |= (1u << bit);
            }
            return mask;
        }

        const LayerDef& LayerTable::GetByIndex(unsigned int index) const
        {
            return mLayers[index];
        }

        const LayerDef& LayerTable::GetById(Dia::Core::StringCRC id) const
        {
            int idx = FindIndex(id);
            return mLayers[(idx >= 0) ? static_cast<unsigned int>(idx) : 0u];
        }

        bool LayerTable::Has(Dia::Core::StringCRC id) const
        {
            return FindIndex(id) >= 0;
        }

        int LayerTable::FindIndex(Dia::Core::StringCRC id) const
        {
            for (unsigned int i = 0; i < mCount; ++i)
            {
                if (mLayers[i].id == id)
                    return static_cast<int>(i);
            }
            return -1;
        }

    } // namespace Scene2D
} // namespace Dia
