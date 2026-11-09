// S-1 World Generation map view: the narrow interface WorldGenDebugDomain uses to drive the hosted
// DiaCellLayers viewer (selection, colour mode, overlays, legend). The viewer is the single owner of
// selection and colour mode; the debug domain only forwards through this interface.
// SimPU only: every call must come from the thread that owns the viewer.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgen-view.plan.md (tasks 7, 8)
#pragma once

#include <DiaCore/Json/external/json/json.h>

#include <functional>

namespace CoW {

class IWorldGenView
{
public:
    virtual ~IWorldGenView() {}

    // Replaces the selection with one cell. False when (x, y) is outside the grid or the viewer is not built.
    virtual bool SelectCell(int x, int y) = 0;
    virtual void ClearSelection() = 0;
    virtual bool HasSelection() = 0;
    // First selected cell (row-major order); false when nothing is selected.
    virtual bool GetSelection(int& x, int& y) = 0;

    // Active base layer id. "terrain" is accepted as an alias of the default base layer ("relief").
    // False (and no change) for an unknown id.
    virtual bool SetColourMode(const char* id) = 0;
    // Active base layer id as registered ("relief", "elevation", ...). Never null.
    virtual const char* GetColourMode() = 0;
    // Calls back once per colour mode (every registered base layer id), in registration order.
    virtual void ListColourModes(const std::function<void(const char* id)>& fn) = 0;

    // Overlay ids: "rivers", "selection.neighbours". False for an unknown id.
    virtual bool SetOverlayEnabled(const char* id, bool enabled) = 0;
    // Calls back once per overlay. available = the overlay has something to draw right now.
    virtual void ListOverlays(const std::function<void(const char* id, bool enabled, bool available)>& fn) = 0;

    // view.legend for the active base layer: a JSON array of {label, colour:"#rrggbb"} for category layers,
    // or an object {min, max, ramp, unit, stops:[{t, colour}]} for float layers. Null when no legend yet.
    virtual void FillLegend(Json::Value& out) = 0;
};

} // namespace CoW
