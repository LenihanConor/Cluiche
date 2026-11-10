////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldViewOrder.h
// CellField view id and 2D view ordering (bgfx-free so Canvas and tests share it).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <stddef.h>

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

        // bgfx::setViewOrder(start, num, remap) remaps only [start, start+num). The 2D views are
        // contiguous 4..8 (Canvas3D keeps 0-3 untouched); the remap places the cell field between
        // entity and debug.
        constexpr unsigned short kCellFieldViewOrderStart = 4;
        constexpr size_t kCellFieldViewOrderCount = 5;

        inline std::array<unsigned short, kCellFieldViewOrderCount> BuildCellFieldViewRemap(
            unsigned short entityView, unsigned short cellFieldView, unsigned short debugView,
            unsigned short uiView, unsigned short imguiView)
        {
            return BuildCellFieldViewOrder(entityView, cellFieldView, debugView, uiView, imguiView);
        }

        // True when `remap` is a permutation of [start, start+count) (what bgfx::setViewOrder requires).
        inline bool IsViewOrderPermutation(const unsigned short* remap, size_t count, unsigned short start)
        {
            bool seen[kCellFieldViewOrderCount] = {};
            if (count > kCellFieldViewOrderCount) return false;
            for (size_t i = 0; i < count; ++i)
            {
                if (remap[i] < start || remap[i] >= start + count) return false;
                if (seen[remap[i] - start]) return false;
                seen[remap[i] - start] = true;
            }
            return true;
        }

    } // namespace Bgfx
} // namespace Dia
