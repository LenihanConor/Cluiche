#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace AssetRuntime
    {
        class DiaAssetRuntimeVisualDebugger : public Dia::Debug::IVisualDebugger
        {
        public:
            Dia::Core::StringCRC GetLayerName() const override;
            void Draw(Dia::Core::IDebugDraw& draw) override;
            void DrawImGui() override;
        };

    } // namespace AssetRuntime
} // namespace Dia

#endif // DIA_DEBUG
