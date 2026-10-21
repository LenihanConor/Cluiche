////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldViewOrder.h
// CellField view id and 2D view ordering (bgfx-free so Canvas and tests share it).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>

namespace Dia
{
    namespace Bgfx
    {
        // CellField draws in its own view, ordered between the entity and debug views.
        // The renderer sets this view to Sequential so backdrop-then-layer submit order holds.
        constexpr unsigned short kCellFieldViewId = 8;

        // Draw order of the 2D views: entity, cell field, debug, UI, ImGui.
        inline std::array<unsigned short, 5> BuildCellFieldViewOrder(unsigned short entityView, unsigned short cellFieldView,
                                                                      unsigned short debugView, unsigned short uiView,
                                                                      unsigned short imguiView)
        {
            return { entityView, cellFieldView, debugView, uiView, imguiView };
        }

    } // namespace Bgfx
} // namespace Dia
