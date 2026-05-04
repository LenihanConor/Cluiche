////////////////////////////////////////////////////////////////////////////////
// Filename: IImGuiBackend.h
// Description: Pure virtual interface for ImGui backend lifecycle management.
//              Each renderer (SFML, SDL, DX12) implements this interface.
//              ProcessEvent is deliberately NOT on the interface -- event types
//              are renderer-specific.
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
    namespace ImGui
    {
        class IImGuiBackend
        {
        public:
            virtual ~IImGuiBackend() = default;
            virtual void Init()             = 0;
            virtual void Shutdown()         = 0;
            virtual void NewFrame(float dt) = 0;
            virtual void Render()           = 0;
        };

    } // namespace ImGui
} // namespace Dia
