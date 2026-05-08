// debug-layers.js — Debug Layers panel for DiaEditor
// Receives "debug.layer.state" data-update pushed by DebugLayerPanelPlugin
// via WebUIBridge::NotifyUIDataChanged.
//
// Data shape (matches DebugLayerManager::BroadcastLayerState JSON):
//   {
//     "droppedCount": <uint>,
//     "layers": [
//       { "name": <string>, "enabled": <bool>, "priority": <int> },
//       ...
//     ]
//   }
//
// Sends toggle commands back via window.parent postMessage, which the
// WebUIBridge routes to GameConnectionManager::SendCommand().
(function () {
    var badgeEl    = document.getElementById("drop-badge");
    var bannerEl   = document.getElementById("overflow-banner");
    var listEl     = document.getElementById("layer-list");
    var emptyEl    = document.getElementById("empty-msg");

    // Listen for DiaEditor_onDataChanged pushed from the C++ side.
    window.addEventListener("message", function (e) {
        var msg = e.data || {};
        // Topic-envelope format used by WebUIBridge::NotifyUIDataChanged
        if (!msg.__dia) return;
        if (msg.topic === "debug.layer.state") {
            applyLayerState(msg.data || {});
        }
    });

    function sendCommand(command, args) {
        try {
            window.parent.postMessage({
                __diaFromFrame: true,
                payload: { type: command, data: args || {} }
            }, "*");
        } catch (e) {
            console.warn("[DebugLayers] sendCommand failed:", e);
        }
    }

    function applyLayerState(state) {
        var dropped = (state.droppedCount != null) ? state.droppedCount : 0;
        var layers  = state.layers || [];

        // Overflow badge
        if (dropped > 0) {
            badgeEl.textContent  = dropped + " dropped";
            badgeEl.className    = "badge visible";
            bannerEl.className   = "overflow-banner visible";
        } else {
            badgeEl.className    = "badge";
            bannerEl.className   = "overflow-banner";
        }

        // Layer list
        if (layers.length === 0) {
            listEl.style.display = "none";
            emptyEl.style.display = "";
            return;
        }

        // Sort by priority ascending, then alphabetically within each priority tier
        var sorted = layers.slice().sort(function (a, b) {
            if (a.priority !== b.priority) return a.priority - b.priority;
            return (a.name || "").localeCompare(b.name || "");
        });

        emptyEl.style.display = "none";
        listEl.style.display  = "";
        listEl.innerHTML      = "";

        sorted.forEach(function (layer) {
            var li      = document.createElement("li");
            li.className = "layer-item";

            var cb       = document.createElement("input");
            cb.type      = "checkbox";
            cb.checked   = !!layer.enabled;
            cb.addEventListener("change", function () {
                var cmd = cb.checked
                    ? "debug.layer.enable"
                    : "debug.layer.disable";
                sendCommand(cmd, { name: layer.name });
            });

            var nameSpan = document.createElement("span");
            nameSpan.className = "layer-name" + (layer.enabled ? "" : " disabled");
            nameSpan.textContent = layer.name || "(unnamed)";

            var prioSpan = document.createElement("span");
            prioSpan.className   = "layer-priority";
            prioSpan.textContent = layer.priority != null ? String(layer.priority) : "";

            li.appendChild(cb);
            li.appendChild(nameSpan);
            li.appendChild(prioSpan);
            listEl.appendChild(li);
        });
    }

    console.log("[DebugLayers] Panel loaded.");
})();
