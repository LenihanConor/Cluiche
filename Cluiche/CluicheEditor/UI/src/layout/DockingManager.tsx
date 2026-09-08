import React, { useCallback, useEffect, useMemo, useState } from "react";
import { Mosaic, MosaicWindow, MosaicNode } from "react-mosaic-component";
import "react-mosaic-component/react-mosaic-component.css";
import { EditorBridge, PanelInfo } from "../bridge/EditorBridge";
import { Toolbar } from "./Toolbar";

type PanelId = string;

type TabGroup = { name: string; panels: PanelId[]; activeId: PanelId };
type TabGroupMap = Map<string, TabGroup>;
type CtxMenu = { panelId: PanelId; isGroup: boolean; x: number; y: number } | null;

// ── Tree algorithms ───────────────────────────────────────────────────────────

function buildTree(panelNames: PanelId[]): MosaicNode<PanelId> | null {
  if (panelNames.length === 0) return null;
  if (panelNames.length === 1) return panelNames[0];
  const mid = Math.ceil(panelNames.length / 2);
  const left = buildTree(panelNames.slice(0, mid));
  const right = buildTree(panelNames.slice(mid));
  if (left == null) return right;
  if (right == null) return left;
  return { direction: panelNames.length > 2 ? "column" : "row", first: left, second: right, splitPercentage: 50 };
}

function collectLeaves(node: MosaicNode<PanelId> | null): PanelId[] {
  if (node == null) return [];
  if (typeof node === "string") return [node];
  return [...collectLeaves(node.first), ...collectLeaves(node.second)];
}

function dedupTree(node: MosaicNode<PanelId> | null, seen: Set<PanelId> = new Set()): MosaicNode<PanelId> | null {
  if (node == null) return null;
  if (typeof node === "string") {
    if (seen.has(node)) return null;
    seen.add(node);
    return node;
  }
  const first = dedupTree(node.first, seen);
  const second = dedupTree(node.second, seen);
  if (first == null) return second;
  if (second == null) return first;
  return { ...node, first, second };
}

function removeFromLayout(node: MosaicNode<PanelId> | null, id: PanelId): MosaicNode<PanelId> | null {
  if (node == null) return null;
  if (typeof node === "string") return node === id ? null : node;
  const first = removeFromLayout(node.first, id);
  const second = removeFromLayout(node.second, id);
  if (first == null) return second;
  if (second == null) return first;
  return { ...node, first, second };
}

function addToLayout(node: MosaicNode<PanelId> | null, id: PanelId): MosaicNode<PanelId> {
  if (node == null) return id;
  return { direction: "row", first: node, second: id, splitPercentage: 70 };
}

function replaceInLayout(node: MosaicNode<PanelId> | null, oldId: PanelId, newId: PanelId): MosaicNode<PanelId> | null {
  if (node == null) return null;
  if (typeof node === "string") return node === oldId ? newId : node;
  return { ...node, first: replaceInLayout(node.first, oldId, newId) ?? node.first, second: replaceInLayout(node.second, oldId, newId) ?? node.second };
}

function isGroupId(id: PanelId): boolean { return id.startsWith("tabgroup:"); }
function newGroupId(): string { return `tabgroup:${Math.random().toString(36).slice(2, 8)}`; }

// ── AddPanelButton sub-component ──────────────────────────────────────────────

function AddPanelButton({ panels, onAdd }: { panels: PanelInfo[]; onAdd: (id: PanelId) => void }) {
  const [open, setOpen] = useState(false);

  useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      if (!(e.target as Element)?.closest("[data-add-panel-btn]")) setOpen(false);
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, [open]);

  if (panels.length === 0) return null;
  return (
    <div data-add-panel-btn="" style={{ position: "relative", display: "flex", alignItems: "center", padding: "0 2px" }}>
      <button
        onClick={() => setOpen(o => !o)}
        title="Add panel to group"
        style={{ background: "transparent", border: "none", cursor: "pointer", color: open ? "#ccc" : "#666", fontSize: 16, lineHeight: 1, padding: "0 4px", borderRadius: 2 }}
        onMouseEnter={e => { e.currentTarget.style.color = "#ccc"; }}
        onMouseLeave={e => { if (!open) e.currentTarget.style.color = "#666"; }}
      >
        +
      </button>
      {open && (
        <div style={{ position: "absolute", top: "100%", left: 0, background: "#252526", border: "1px solid #3c3c3c", borderRadius: 4, minWidth: 160, boxShadow: "0 4px 12px rgba(0,0,0,0.5)", zIndex: 1000 }}>
          <div style={{ padding: "4px 10px 3px", fontSize: 10, textTransform: "uppercase", letterSpacing: "0.8px", color: "#858585", borderBottom: "1px solid #3c3c3c" }}>
            Add to group
          </div>
          {panels.map(p => (
            <div key={p.name} onClick={() => { onAdd(p.name); setOpen(false); }}
              style={{ padding: "5px 12px", fontSize: 12, color: "#ccc", cursor: "pointer" }}
              onMouseEnter={e => { e.currentTarget.style.background = "#04395e"; }}
              onMouseLeave={e => { e.currentTarget.style.background = "transparent"; }}>
              {p.name}
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

// ── DockingManager ────────────────────────────────────────────────────────────

interface DockingManagerProps {
  onReady?: () => void;
}

export function DockingManager({ onReady }: DockingManagerProps) {
  const [panels, setPanels] = useState<PanelInfo[]>([]);
  const [layout, setLayout] = useState<MosaicNode<PanelId> | null>(null);
  const [savedLayout, setSavedLayout] = useState<MosaicNode<PanelId> | null>(null);
  const [tabGroups, setTabGroups] = useState<TabGroupMap>(new Map());
  const [savedTabGroups, setSavedTabGroups] = useState<TabGroupMap | null>(null);
  const [initialized, setInitialized] = useState(false);
  const [fullscreenPanel, setFullscreenPanel] = useState<PanelId | null>(null);
  const [contextMenu, setContextMenu] = useState<CtxMenu>(null);

  const panelMap = useMemo(() => {
    const m = new Map<PanelId, PanelInfo>();
    panels.forEach(p => m.set(p.name, p));
    return m;
  }, [panels]);

  const groupedPanelIds = useMemo(() => {
    const ids = new Set<PanelId>();
    tabGroups.forEach(g => g.panels.forEach(p => ids.add(p)));
    return ids;
  }, [tabGroups]);

  const loosePanelIds = useMemo(() => collectLeaves(layout).filter(id => !isGroupId(id)), [layout]);

  // Panels visible but not yet in the mosaic (neither loose leaf nor grouped)
  const ungroupedAddable = useMemo(() =>
    panels.filter(p => p.visible && !loosePanelIds.includes(p.name) && !groupedPanelIds.has(p.name)),
    [panels, loosePanelIds, groupedPanelIds]
  );

  function saveState(tree: MosaicNode<PanelId> | null, groups: TabGroupMap) {
    if (!tree) return;
    const tgObj: Record<string, TabGroup> = {};
    groups.forEach((g, k) => { tgObj[k] = g; });
    EditorBridge.saveLayout({ tree, tabGroups: tgObj }).catch(() => {
      EditorBridge.notify({ level: "warning", title: "Failed to save layout" });
    });
  }

  // ── Initialization ──────────────────────────────────────────────────────────
  useEffect(() => {
    EditorBridge.getPanels()
      .then(res => {
        const list = res?.panels ?? [];
        setPanels(list);
        EditorBridge.loadLayout()
          .then(saved => {
            const s = saved as { tree?: MosaicNode<PanelId>; tabGroups?: Record<string, TabGroup> } | null;
            if (s?.tree) {
              const clean = dedupTree(s.tree);
              const knownPanelNames = new Set(list.map(p => p.name));

              // Reconstruct groups, pruning any that reference panels no longer registered.
              const rawGroups: TabGroupMap = new Map();
              if (s.tabGroups) Object.entries(s.tabGroups).forEach(([k, v]) => rawGroups.set(k, v));

              const groups: TabGroupMap = new Map();
              rawGroups.forEach((g, gid) => {
                const validPanels = g.panels.filter(p => knownPanelNames.has(p));
                if (validPanels.length === 0) return; // drop empty group entirely
                const activeId = validPanels.includes(g.activeId) ? g.activeId : validPanels[0];
                groups.set(gid, { ...g, panels: validPanels, activeId });
              });

              // Remove any tabgroup leaves whose group entry was dropped during pruning.
              let safeTree = clean;
              if (clean) {
                collectLeaves(clean).forEach(leaf => {
                  if (isGroupId(leaf) && !groups.has(leaf)) {
                    safeTree = removeFromLayout(safeTree, leaf);
                  }
                });
              }

              setLayout(safeTree);
              setTabGroups(groups);
              if (safeTree) saveState(safeTree, groups);
            } else {
              const visible = list.filter(p => p.visible).map(p => p.name);
              setLayout(buildTree(visible));
            }
          })
          .catch(() => {
            const visible = list.filter(p => p.visible).map(p => p.name);
            setLayout(buildTree(visible));
          })
          .finally(() => { setInitialized(true); onReady?.(); });
      })
      .catch(() => { setLayout(null); setInitialized(true); onReady?.(); });
  }, []);

  // ── is-resizing cursor helper ───────────────────────────────────────────────
  useEffect(() => {
    const onMouseDown = (e: MouseEvent) => {
      if ((e.target as Element)?.closest(".mosaic-split")) {
        document.body.classList.add("is-resizing");
        document.addEventListener("mouseup", () => document.body.classList.remove("is-resizing"), { once: true });
      }
    };
    document.addEventListener("mousedown", onMouseDown);
    return () => document.removeEventListener("mousedown", onMouseDown);
  }, []);

  // ── panels_changed ──────────────────────────────────────────────────────────
  useEffect(() => {
    return EditorBridge.subscribe("panels_changed", (data: unknown) => {
      const d = data as { panels?: PanelInfo[] } | null;
      if (!d?.panels) return;
      const newPanels = d.panels;
      setPanels(prevPanels => {
        const prevNames = new Set(prevPanels.map(p => p.name));
        const newNames = new Set(newPanels.map(p => p.name));
        const added = newPanels.filter(p => !prevNames.has(p.name) && p.visible);
        const removed = [...prevNames].filter(n => !newNames.has(n));
        const toggledOn = newPanels.filter(p => { const prev = prevPanels.find(pp => pp.name === p.name); return prev && !prev.visible && p.visible; });
        const toggledOff = newPanels.filter(p => { const prev = prevPanels.find(pp => pp.name === p.name); return prev && prev.visible && !p.visible; });
        const toRemove = new Set([...removed, ...toggledOff.map(p => p.name)]);
        const toAdd = [...added, ...toggledOn].map(p => p.name);
        if (toRemove.size > 0 || toAdd.length > 0) {
          setTabGroups(prevGroups => {
            let nextGroups = prevGroups;
            toRemove.forEach(panelId => {
              for (const [gid, group] of nextGroups) {
                if (!group.panels.includes(panelId)) continue;
                if (nextGroups === prevGroups) nextGroups = new Map(prevGroups);
                const remaining = group.panels.filter(p => p !== panelId);
                if (remaining.length === 0) {
                  nextGroups.delete(gid);
                } else {
                  const newActive = group.activeId === panelId ? remaining[0] : group.activeId;
                  nextGroups.set(gid, { ...group, panels: remaining, activeId: newActive });
                }
                break;
              }
            });
            setLayout(prev => {
              let updated = prev;
              // Remove empty group tiles whose groups were just deleted
              if (nextGroups !== prevGroups) {
                for (const gid of prevGroups.keys()) {
                  if (!nextGroups.has(gid)) updated = removeFromLayout(updated, gid);
                }
              }
              toRemove.forEach(id => { updated = removeFromLayout(updated, id); });
              toAdd.forEach(id => { updated = addToLayout(updated, id); });
              return updated;
            });
            return nextGroups;
          });
        }
        return newPanels;
      });
    });
  }, []);

  // ── Dismiss context menu on outside click ──────────────────────────────────
  useEffect(() => {
    if (!contextMenu) return;
    const handler = (e: MouseEvent) => {
      if (!(e.target as Element)?.closest("[data-ctx-menu]")) setContextMenu(null);
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, [contextMenu]);

  // ── Layout change ───────────────────────────────────────────────────────────
  const handleChange = useCallback((newLayout: MosaicNode<PanelId> | null) => {
    setLayout(newLayout);
    setTabGroups(prev => { if (newLayout) saveState(newLayout, prev); return prev; });
  }, []);

  // ── Fullscreen ──────────────────────────────────────────────────────────────
  const handleFullscreen = useCallback((id: PanelId) => {
    setFullscreenPanel(prev => {
      if (prev === id) {
        setSavedLayout(saved => { setLayout(saved); return null; });
        setSavedTabGroups(saved => { if (saved) setTabGroups(saved); return null; });
        return null;
      }
      setLayout(current => { setSavedLayout(current); return current; });
      setTabGroups(current => { setSavedTabGroups(new Map(current)); return current; });
      return id;
    });
  }, []);

  // ── Panel/group close ───────────────────────────────────────────────────────
  const handlePanelClose = useCallback((id: PanelId) => {
    if (isGroupId(id)) {
      // Ungroup: return all member panels as loose leaves in the tree
      setTabGroups(prev => {
        const group = prev.get(id);
        const next = new Map(prev);
        next.delete(id);
        setLayout(prevLayout => {
          let updated = removeFromLayout(prevLayout, id);
          group?.panels.forEach(panelId => { updated = addToLayout(updated, panelId); });
          saveState(updated, next);
          return updated;
        });
        return next;
      });
    } else {
      if (fullscreenPanel === id) setFullscreenPanel(null);
      EditorBridge.togglePanelVisibility(id);
      setLayout(prev => removeFromLayout(prev, id));
    }
  }, [fullscreenPanel]);

  // ── Tab group operations ────────────────────────────────────────────────────

  const handleCreateGroup = useCallback((panelId: PanelId) => {
    setContextMenu(null);
    const groupId = newGroupId();
    setTabGroups(prev => {
      const groupNumber = prev.size + 1;
      const newGroup: TabGroup = { name: `Group ${groupNumber}`, panels: [panelId], activeId: panelId };
      const next = new Map(prev);
      next.set(groupId, newGroup);
      setLayout(prevLayout => {
        const newLayout = replaceInLayout(prevLayout, panelId, groupId);
        saveState(newLayout, next);
        return newLayout;
      });
      return next;
    });
  }, []);

  const handleAddToGroup = useCallback((panelId: PanelId, groupId: string) => {
    setContextMenu(null);
    setTabGroups(prev => {
      const group = prev.get(groupId);
      if (!group || group.panels.includes(panelId)) return prev;
      const next = new Map(prev);
      next.set(groupId, { ...group, panels: [...group.panels, panelId], activeId: panelId });
      setLayout(prevLayout => {
        const newLayout = removeFromLayout(prevLayout, panelId);
        saveState(newLayout, next);
        return newLayout;
      });
      return next;
    });
  }, []);

  const handleRemoveFromGroup = useCallback((panelId: PanelId, groupId: string) => {
    setTabGroups(prev => {
      const group = prev.get(groupId);
      if (!group) return prev;
      const next = new Map(prev);
      const remaining = group.panels.filter(p => p !== panelId);
      if (remaining.length === 0) {
        next.delete(groupId);
        setLayout(prevLayout => {
          const withoutGroup = removeFromLayout(prevLayout, groupId);
          const newLayout = addToLayout(withoutGroup, panelId);
          saveState(newLayout, next);
          return newLayout;
        });
      } else {
        const newActive = group.activeId === panelId ? remaining[0] : group.activeId;
        next.set(groupId, { ...group, panels: remaining, activeId: newActive });
        setLayout(prevLayout => {
          const newLayout = addToLayout(prevLayout, panelId);
          saveState(newLayout, next);
          return newLayout;
        });
      }
      return next;
    });
  }, []);

  const handleTabActivate = useCallback((groupId: string, panelId: PanelId) => {
    setTabGroups(prev => {
      const group = prev.get(groupId);
      if (!group || group.activeId === panelId) return prev;
      const next = new Map(prev);
      next.set(groupId, { ...group, activeId: panelId });
      return next;
    });
  }, []);

  const handleRenameGroup = useCallback((groupId: string) => {
    setContextMenu(null);
    setTabGroups(prev => {
      const group = prev.get(groupId);
      if (!group) return prev;
      const newName = window.prompt("Group name:", group.name);
      if (!newName || !newName.trim()) return prev;
      const next = new Map(prev);
      next.set(groupId, { ...group, name: newName.trim() });
      setLayout(prevLayout => { saveState(prevLayout, next); return prevLayout; });
      return next;
    });
  }, []);

  const openContextMenu = useCallback((e: React.MouseEvent, panelId: PanelId, isGroup: boolean) => {
    e.preventDefault();
    e.stopPropagation();
    setContextMenu({ panelId, isGroup, x: e.clientX, y: e.clientY });
  }, []);

  // ── Tile renderers ──────────────────────────────────────────────────────────

  function renderSinglePanelTile(id: PanelId, path: unknown) {
    const info = panelMap.get(id);
    const src = info?.uiPath ?? `dia://editor/${id.toLowerCase().replace(/\s+/g, "-")}/index.html`;
    const isFs = fullscreenPanel === id;
    return (
      <MosaicWindow<PanelId>
        path={path as any}
        title={
          <span
            style={{ flex: 1, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}
            onContextMenu={e => openContextMenu(e, id, false)}
          >
            {id}
          </span>
        }
        createNode={() => panels[0]?.name ?? id}
        toolbarControls={[
          <button key="fs" onClick={() => handleFullscreen(id)} title={isFs ? "Exit fullscreen" : "Fullscreen"}
            className="mosaic-default-control bp4-button bp4-minimal"
            style={{ background: "transparent", border: "none", cursor: "pointer", color: "#999", fontSize: 12, lineHeight: 1, padding: "0 4px" }}>
            {isFs ? "⊡" : "⊞"}
          </button>,
          <button key="close" onClick={() => handlePanelClose(id)} title="Hide panel"
            className="mosaic-default-control bp4-button bp4-minimal"
            style={{ background: "transparent", border: "none", cursor: "pointer", color: "#999", fontSize: 14, lineHeight: 1, padding: "0 4px" }}>
            ×
          </button>,
        ]}
      >
        <iframe key={id} src={src} style={{ width: "100%", height: "100%", border: "none" }} title={id} />
      </MosaicWindow>
    );
  }

  function renderTabGroupTile(groupId: PanelId, path: unknown) {
    const group = tabGroups.get(groupId);
    if (!group) return null;

    const addable = panels.filter(p => p.visible && !group.panels.includes(p.name) && !groupedPanelIds.has(p.name));
    const activeSrc = (() => {
      const info = panelMap.get(group.activeId);
      return info?.uiPath ?? `dia://editor/${group.activeId.toLowerCase().replace(/\s+/g, "-")}/index.html`;
    })();

    return (
      <MosaicWindow<PanelId>
        path={path as any}
        title={
          <span
            style={{ flex: 1, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}
            onContextMenu={e => openContextMenu(e, groupId, true)}
          >
            {group.name}
          </span>
        }
        createNode={() => panels[0]?.name ?? groupId}
        toolbarControls={[
          <button key="close" onClick={() => handlePanelClose(groupId)} title="Ungroup panels"
            className="mosaic-default-control bp4-button bp4-minimal"
            style={{ background: "transparent", border: "none", cursor: "pointer", color: "#999", fontSize: 14, lineHeight: 1, padding: "0 4px" }}>
            ×
          </button>,
        ]}
      >
        <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
          {/* Tab bar */}
          <div style={{ display: "flex", alignItems: "stretch", height: 30, background: "#252526", borderBottom: "1px solid #3c3c3c", flexShrink: 0 }}>
            {group.panels.map(panelId => (
              <div
                key={panelId}
                onClick={() => handleTabActivate(groupId, panelId)}
                style={{
                  display: "flex", alignItems: "center", gap: 4, padding: "0 10px",
                  cursor: "pointer", fontSize: 12, userSelect: "none",
                  color: panelId === group.activeId ? "#ccc" : "#858585",
                  background: panelId === group.activeId ? "#1e1e1e" : "transparent",
                  borderRight: "1px solid #3c3c3c",
                  borderBottom: panelId === group.activeId ? "2px solid #007acc" : "2px solid transparent",
                }}
              >
                <span>{panelId}</span>
                <button
                  onClick={e => { e.stopPropagation(); handleRemoveFromGroup(panelId, groupId); }}
                  title="Remove from group"
                  style={{ background: "transparent", border: "none", cursor: "pointer", color: "#555", fontSize: 12, padding: "0 2px", lineHeight: 1, borderRadius: 2 }}
                  onMouseEnter={e => { e.currentTarget.style.background = "rgba(255,255,255,0.1)"; e.currentTarget.style.color = "#ccc"; }}
                  onMouseLeave={e => { e.currentTarget.style.background = "transparent"; e.currentTarget.style.color = "#555"; }}
                >
                  ×
                </button>
              </div>
            ))}
            <AddPanelButton panels={addable} onAdd={panelId => handleAddToGroup(panelId, groupId)} />
          </div>
          {/* Active panel iframe */}
          <iframe key={group.activeId} src={activeSrc} style={{ flex: 1, width: "100%", border: "none" }} title={group.activeId} />
        </div>
      </MosaicWindow>
    );
  }

  function renderTile(id: PanelId, path: unknown) {
    if (isGroupId(id)) return renderTabGroupTile(id, path) ?? <div />;
    return renderSinglePanelTile(id, path);
  }

  function renderContextMenu() {
    if (!contextMenu) return null;
    const { panelId, isGroup, x, y } = contextMenu;
    const existingGroups = [...tabGroups.entries()];

    return (
      <div data-ctx-menu="" style={{ position: "fixed", left: x, top: y, background: "#252526", border: "1px solid #3c3c3c", borderRadius: 4, minWidth: 220, boxShadow: "0 6px 20px rgba(0,0,0,0.6)", zIndex: 2000, overflow: "hidden" }}>
        {isGroup ? (
          <>
            <CtxItem label="Rename group…" onClick={() => handleRenameGroup(panelId)} />
            <div style={{ height: 1, background: "#3c3c3c", margin: "2px 0" }} />
            <CtxItem label="Ungroup all" onClick={() => handlePanelClose(panelId)} />
          </>
        ) : (
          <>
            {existingGroups.map(([gid, g]) => (
              <CtxItem key={gid} label={`Add to "${g.name}"`} onClick={() => handleAddToGroup(panelId, gid)} />
            ))}
            {existingGroups.length > 0 && <div style={{ height: 1, background: "#3c3c3c", margin: "2px 0" }} />}
            <CtxItem label="New tab group here" onClick={() => handleCreateGroup(panelId)} />
          </>
        )}
      </div>
    );
  }

  // ── Render ──────────────────────────────────────────────────────────────────

  if (!initialized) {
    return <div style={{ padding: 16, color: "#888", fontFamily: "monospace" }}>Loading panels…</div>;
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div style={{ flex: 1, position: "relative" }}>
        {fullscreenPanel ? (
          <div style={{ width: "100%", height: "100%", display: "flex", flexDirection: "column" }}>
            <div style={{ display: "flex", alignItems: "center", height: 30, background: "#1e1e1e", borderBottom: "1px solid #3c3c3c", padding: "0 8px", flexShrink: 0 }}>
              <span style={{ flex: 1, fontSize: 12, color: "#ccc", fontFamily: "Segoe UI, system-ui, sans-serif" }}>
                {isGroupId(fullscreenPanel) ? (tabGroups.get(fullscreenPanel)?.name ?? fullscreenPanel) : fullscreenPanel}
              </span>
              <button onClick={() => handleFullscreen(fullscreenPanel)} title="Exit fullscreen"
                style={{ background: "transparent", border: "none", cursor: "pointer", color: "#999", fontSize: 12, lineHeight: 1, padding: "0 4px" }}>
                ⊡
              </button>
            </div>
            <iframe
              key={fullscreenPanel}
              src={panelMap.get(fullscreenPanel)?.uiPath ?? `dia://editor/${fullscreenPanel.toLowerCase().replace(/\s+/g, "-")}/index.html`}
              style={{ flex: 1, width: "100%", border: "none" }}
              title={fullscreenPanel}
            />
          </div>
        ) : layout ? (
          <Mosaic<PanelId> className="mosaic-blueprint-theme bp4-dark" renderTile={renderTile} value={layout} onChange={handleChange} />
        ) : (
          <div style={{ padding: 16, color: "#888", fontFamily: "monospace" }}>
            No editor panels visible. Use the toolbar to show a panel.
          </div>
        )}
      </div>
      {renderContextMenu()}
      <Toolbar panels={panels} />
    </div>
  );
}

// ── CtxItem helper ─────────────────────────────────────────────────────────────

function CtxItem({ label, onClick }: { label: string; onClick: () => void }) {
  return (
    <div onClick={onClick}
      style={{ padding: "6px 14px", fontSize: 12, color: "#ccc", cursor: "pointer" }}
      onMouseEnter={e => { e.currentTarget.style.background = "#094771"; }}
      onMouseLeave={e => { e.currentTarget.style.background = "transparent"; }}>
      {label}
    </div>
  );
}
