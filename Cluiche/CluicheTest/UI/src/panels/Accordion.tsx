import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

type AccordionSection = { id: string; title: string; content: string };

export function Accordion() {
  const [sections, setSections] = useState<AccordionSection[]>([]);
  const [openId, setOpenId] = useState<string | null>(null);

  useEffect(() => {
    return GameBridge.subscribe("accordion", (data) => {
      const d = data as { sections: AccordionSection[] };
      setSections(d.sections ?? []);
    });
  }, []);

  function toggle(id: string) {
    const next = openId === id ? null : id;
    setOpenId(next);
    GameBridge.send("onAccordionToggled", { id, open: next === id });
  }

  return (
    <div className="flex flex-col gap-0 p-3 bg-base-200 rounded">
      <span className="text-xs font-bold uppercase tracking-widest opacity-60 mb-2">Accordion</span>

      {sections.length === 0 ? (
        <p className="text-xs opacity-40">Waiting for data…</p>
      ) : (
        sections.map((s) => (
          <div key={s.id} className="collapse collapse-arrow bg-base-300 mb-1 rounded">
            <input
              type="radio"
              name="accordion"
              checked={openId === s.id}
              onChange={() => toggle(s.id)}
            />
            <div className="collapse-title text-sm font-medium py-2 min-h-0">
              {s.title}
            </div>
            <div className="collapse-content text-xs">
              <p className="pt-1">{s.content}</p>
            </div>
          </div>
        ))
      )}
    </div>
  );
}
