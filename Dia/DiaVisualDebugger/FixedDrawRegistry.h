////////////////////////////////////////////////////////////////////////////////
// Filename: FixedDrawRegistry.h
// Description: Internal registry for fixed-topology debug objects.
//              Not part of the public API — accessed only through DebugLayerManager.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/FixedPrimitiveBuffer.h>

namespace Dia
{
    namespace Graphics
    {
        class DebugFrameDataVisitor;
    }
}

namespace Dia
{
    namespace Debug
    {
        class FixedDrawRegistry
        {
        public:
            static const unsigned int kMaxFixed = 32;

            FixedDrawRegistry() = default;
            ~FixedDrawRegistry();

            void Register(Dia::Core::StringCRC name,
                          const void*          sourceObject,
                          IObjectRenderer*     renderer,
                          unsigned int         capacity,
                          int                  priority);

            void Unregister(Dia::Core::StringCRC name);

            void EnableLayer (Dia::Core::StringCRC name);
            void DisableLayer(Dia::Core::StringCRC name);
            bool IsLayerEnabled(Dia::Core::StringCRC name) const;
            bool HasLayer(Dia::Core::StringCRC name) const;

            void Invalidate(Dia::Core::StringCRC name);

            void DrawFixed(const Dia::Graphics::DebugFrameDataVisitor& visitor);

            int GetCount() const { return static_cast<int>(mEntries.Size()); }

        private:
            struct FixedEntry
            {
                Dia::Core::StringCRC  name;
                const void*           sourceObject = nullptr;
                IObjectRenderer*      renderer     = nullptr;
                FixedPrimitiveBuffer* buffer       = nullptr;
                int                   priority     = 0;
                bool                  enabled      = true;
                bool                  dirty        = true;
            };

            Dia::Core::Containers::DynamicArrayC<FixedEntry, kMaxFixed> mEntries;
            bool mSortDirty = false;
            bool mIsDrawing = false;

            void RebuildEntry(FixedEntry& entry);
            void SortByPriority();
            int  FindIndex(Dia::Core::StringCRC name) const;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
