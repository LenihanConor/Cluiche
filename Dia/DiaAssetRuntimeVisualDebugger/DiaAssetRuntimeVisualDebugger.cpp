#include "DiaAssetRuntimeVisualDebugger/DiaAssetRuntimeVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>

namespace Dia
{
    namespace AssetRuntime
    {

Dia::Core::StringCRC DiaAssetRuntimeVisualDebugger::GetLayerName() const
{
    return Dia::Debug::LayerNames::kAssetRuntime;
}

void DiaAssetRuntimeVisualDebugger::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
    DIA_TRACE_ZONE("asset.runtime", ::Dia::Observation::Trace::Category::kDiaGraphics);
}

    } // namespace AssetRuntime
} // namespace Dia

#endif // DIA_DEBUG
