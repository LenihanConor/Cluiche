////////////////////////////////////////////////////////////////////////////////
// Filename: IDebugLayerRegistry.h
// Description: Neutral debug-layer registration contract. Lets domain visual
//              debuggers register/unregister drawers and toggle layer state
//              without depending on the concrete, graphics-touching
//              DebugLayerManager (Camera2D/ViewportTransform/Camera3D).
//              Extends IDebugContext since every implementor already provides
//              it and most domain debuggers use both facets through one
//              cached reference. Implementor: Dia::Debug::DebugLayerManager.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/DebugDraw/IDebugContext.h>

namespace Dia
{
    namespace Debug
    {
        class IVisualDebugger;

        class IDebugLayerRegistry : public Dia::Core::IDebugContext
        {
        public:
            virtual void Register(IVisualDebugger* debugger, int priority = 0) = 0;
            virtual void Register(IVisualDebugger* debugger, int priority, const Dia::Core::StringCRC& stageTag) = 0;
            virtual void Unregister(Dia::Core::StringCRC layerName) = 0;

            virtual void EnableLayer (Dia::Core::StringCRC layerName) = 0;
            virtual void DisableLayer(Dia::Core::StringCRC layerName) = 0;
            virtual bool IsLayerEnabled(Dia::Core::StringCRC layerName) const = 0;

            virtual void SetDebugScale(float scale) = 0;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
