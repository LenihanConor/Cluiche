import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type TreeNode = { id: string; label: string; children?: TreeNode[] };

function Node({
  node,
  depth,
  selectedId,
  onSelect,
}: {
  node: TreeNode;
  depth: number;
  selectedId: string | null;
  onSelect: (id: string) => void;
}) {
  const [expanded, setExpanded] = useState(true);
  const hasChildren = node.children && node.children.length > 0;

  return (
    <div>
      <div
        className={`flex items-center gap-1 cursor-pointer rounded px-1 py-0.5 text-xs hover:bg-base-300 ${selectedId === node.id ? "bg-primary text-primary-content" : ""}`}
        style={{ paddingLeft: `${depth * 12 + 4}px` }}
        onClick={() => onSelect(node.id)}
      >
        {hasChildren ? (
          <span
            className="opacity-60 w-3 text-center select-none"
            onClick={(e) => { e.stopPropagation(); setExpanded((v) => !v); }}
          >
            {expanded ? "▾" : "▸"}
          </span>
        ) : (
          <span className="w-3" />
        )}
        <span>{node.label}</span>
      </div>
      {hasChildren && expanded && node.children!.map((child) => (
        <Node key={child.id} node={child} depth={depth + 1} selectedId={selectedId} onSelect={onSelect} />
      ))}
    </div>
  );
}

export function TreeView() {
  const [roots, setRoots] = useState<TreeNode[]>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);

  useEffect(() => {
    return GameBridge.subscribe("tree_view", (data) => {
      setRoots((data as { nodes: TreeNode[] }).nodes ?? []);
    });
  }, []);

  function handleSelect(id: string) {
    setSelectedId(id);
    GameBridge.send("onTreeNodeSelected", { id });
  }

  return (
    <div className="flex flex-col gap-1 p-3 bg-base-200 rounded h-full overflow-auto">
      <span className="text-xs font-bold uppercase tracking-widest opacity-60 mb-1">Tree View</span>
      {roots.length === 0 ? (
        <p className="text-xs opacity-40">Waiting for data…</p>
      ) : (
        roots.map((r) => (
          <Node key={r.id} node={r} depth={0} selectedId={selectedId} onSelect={handleSelect} />
        ))
      )}
    </div>
  );
}
