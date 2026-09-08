////////////////////////////////////////////////////////////////////////////////
// Filename: LayerTable.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaScene2D/Scene2D.h>
#include <stdint.h>

namespace Dia
{
    namespace Scene2D
    {
        ////////////////////////////////////////////////////////////
        /// \brief Resolves layer names to uint32 bitmask indices.
        ///
        /// Built from a Scene2D's layers array. Bit 0 is always the
        /// "default" layer — injected if not explicitly declared.
        /// Max 32 layers (uint32_t bitmask).
        ////////////////////////////////////////////////////////////
        class LayerTable
        {
        public:
            static constexpr unsigned int kMaxLayers     = 32;
            static constexpr const char*  kDefaultLayerId = "default";

            LayerTable();

            // Build from a scene's layers array. Injects "default" at bit 0 if absent.
            void Build(const Dia::Core::Containers::DynamicArrayC<LayerDef, 32>& layers);

            // Returns bit index for the given layer id. Returns 0 ("default") if not found.
            unsigned int GetBitIndex(Dia::Core::StringCRC layerId) const;

            // Resolves an array of layer names to a uint32 bitmask.
            uint32_t ResolveMask(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& layerNames) const;

            unsigned int    GetCount()                    const { return mCount; }
            const LayerDef& GetByIndex(unsigned int index) const;
            const LayerDef& GetById(Dia::Core::StringCRC id) const;
            bool            Has(Dia::Core::StringCRC id)   const;

        private:
            LayerDef     mLayers[kMaxLayers];
            unsigned int mCount = 0;

            int FindIndex(Dia::Core::StringCRC id) const;
        };

    } // namespace Scene2D
} // namespace Dia
