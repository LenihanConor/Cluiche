#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace AssetRuntime
    {
        class DiaAssetRuntimeVisualDebugger : public Dia::Debug::IVisualDebugger
        {
        public:
            Dia::Core::StringCRC GetLayerName() const override;
            void Draw(Dia::Graphics::FrameData& frameData) override;
            void DrawImGui() override;
        };

    } // namespace AssetRuntime
} // namespace Dia

#endif // DIA_DEBUG
