// S-1 World Generation map view: the narrow interface WorldGenDebugDomain uses to drive the hosted
// DiaCellLayers viewer (selection, colour mode, overlays, legend). The viewer is the single owner of
// selection and colour mode; the debug domain only forwards through this interface.
// SimPU only: every call must come from the thread that owns the viewer.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgen-view.plan.md (tasks 7, 8)
#pragma once

#include <DiaCore/Json/external/json/json.h>
#include <Shared/WorldGeneration/Sites.h>

#include <cstdint>
#include <functional>
#include <string>

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
    // Same layers with their picker group (the WorldLayerSource group, e.g. "Terrain"). The default groups everything
    // under "Layers"; the real host overrides it.
    virtual void ListColourGroups(const std::function<void(const char* id, const char* group)>& fn)
    {
        ListColourModes([&fn](const char* id) { fn(id, "Layers"); });
    }

    // Overlay ids: "rivers", "selection.neighbours", "regions.edges", "sites" (master for all site kinds) and one
    // "sites.<kind id>" toggle per site kind (built-in node kinds, then registered kinds), then "dynasty.starts" and
    // "guarantee.evidence". False for an unknown id.
    virtual bool SetOverlayEnabled(const char* id, bool enabled) = 0;
    // Calls back once per overlay. available = the overlay has something to draw right now.
    virtual void ListOverlays(const std::function<void(const char* id, bool enabled, bool available)>& fn) = 0;

    // Display name of an overlay id for the panel ("Region edges", "Sites <kind label>", "Dynasty starts", ...). Unknown id: the id.
    virtual std::string OverlayLabel(const char* id) { return id; }

    // Overlay ids also include "dynasty.starts" and "guarantee.evidence" (worldgen-panel-rest task 4).
    // The guarantee whose proof paths the evidence overlay draws (a ValidationResult guarantee id CRC); 0 = none expanded.
    // Violating cells of every failing guarantee are drawn regardless. Kept across regenerate, import and size change.
    virtual void     ExpandGuarantee(uint32_t /*idCrc*/) {}
    virtual uint32_t ExpandedGuarantee() { return 0; }
    // Why the evidence overlay has nothing to draw ("Evidence hidden: ..."), or empty when evidence is fresh or there is no world.
    virtual std::string EvidenceHiddenReason() { return std::string(); }

    // One row per site kind the Sites overlay can draw (AC11): its toggle id, display info (colour, shape), whether the
    // kind's toggle is on and how many sites of it the world holds. Pointers are valid for the callback only.
    struct SiteKindRow
    {
        const char*                              overlay = "";   // "sites.<kind id>"
        const WorldGeneration::SiteKindInfo*     info = nullptr;
        bool                                     enabled = true;
        int                                      count = 0;
    };
    virtual void ListSiteKinds(const std::function<void(const SiteKindRow&)>& /*fn*/) {}

    // view.legend for the active base layer: a JSON array of {label, colour:"#rrggbb"} for category layers,
    // or an object {min, max, ramp, unit, stops:[{t, colour}]} for float layers. Null when no legend yet.
    virtual void FillLegend(Json::Value& out) = 0;
};

} // namespace CoW
