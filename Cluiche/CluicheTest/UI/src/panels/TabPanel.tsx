import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type Tab = { id: string; label: string; content: string };

export function TabPanel() {
  const [tabs, setTabs] = useState<Tab[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);

  useEffect(() => {
    return GameBridge.subscribe("tab_panel", (data) => {
      const d = data as { tabs: Tab[] };
      setTabs(d.tabs ?? []);
      setActiveId((prev) => prev ?? d.tabs[0]?.id ?? null);
    });
  }, []);

  function selectTab(id: string) {
    setActiveId(id);
    GameBridge.send("onTabSelected", { id });
  }

  const active = tabs.find((t) => t.id === activeId);

  return (
    <div className="flex flex-col gap-0 bg-base-200 rounded overflow-hidden">
      <div role="tablist" className="tabs tabs-bordered bg-base-300 px-2 pt-1">
        {tabs.map((t) => (
          <a
            key={t.id}
            role="tab"
            className={`tab tab-sm ${activeId === t.id ? "tab-active" : ""}`}
            onClick={() => selectTab(t.id)}
          >
            {t.label}
          </a>
        ))}
      </div>

      <div className="p-3 text-sm min-h-16">
        {tabs.length === 0 ? (
          <p className="text-xs opacity-40">Waiting for data…</p>
        ) : (
          <p>{active?.content ?? ""}</p>
        )}
      </div>
    </div>
  );
}
