#pragma once
// GridSearch.h - shared vocabulary for the DiaPathfinding grid search helpers.
// Helpers (MultiSourceSteps, FloodRegion, ConnectedComponents, MultiSourceDijkstra,
// PriorityFlood) are added by later tasks and declared in this header.

#include <concepts>
#include <cstdint>

namespace Dia::Pathfinding {
    // Distinct from kNoCell (same value, different meaning): "search never reached this cell".
    constexpr uint32_t kUnreached = 0xFFFFFFFFu;

    // Any graph over dense cell indices 0..CellCount()-1. ForEachNeighbour visits neighbours
    // in a fixed, documented order and accepts any callable taking a uint32_t.
    template<class G>
    concept CIndexGraph = requires(const G& g, uint32_t c) {
        { g.CellCount() } -> std::convertible_to<uint32_t>;
        g.ForEachNeighbour(c, [](uint32_t) {});
    };
}

#include "DiaPathfinding/GridSearch.inl"
