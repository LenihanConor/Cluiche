import { useEffect, useState, createContext, useContext } from "react";
import { GameBridge } from "../bridge/GameBridge";

// Shared selection context — demonstrates cross-panel state pattern
type SelectionContextType = {
  selectedId: string | null;
  setSelectedId: (id: string | null) => void;
};

const SelectionContext = createContext<SelectionContextType>({
  selectedId: null,
  setSelectedId: () => {},
});

type Entity = { id: string; name: string; pos: string; state: string };

function EntityList() {
  const [entities, setEntities] = useState<Entity[]>([]);
  const { selectedId, setSelectedId } = useContext(SelectionContext);

  useEffect(() => {
    return GameBridge.subscribe("entities", (data) => {
      setEntities(data as Entity[]);
    });
  }, []);

  return (
    <div className="entity-list">
      <h3>Entities</h3>
      <ul>
        {entities.map((e) => (
          <li
            key={e.id}
            onClick={() => {
              setSelectedId(e.id);
              GameBridge.send("onEntitySelected", { id: e.id });
            }}
            style={{ fontWeight: selectedId === e.id ? "bold" : "normal", cursor: "pointer" }}
          >
            {e.name}
          </li>
        ))}
      </ul>
    </div>
  );
}

function DetailPanel() {
  const [entities, setEntities] = useState<Entity[]>([]);
  const { selectedId } = useContext(SelectionContext);

  useEffect(() => {
    return GameBridge.subscribe("entities", (data) => {
      setEntities(data as Entity[]);
    });
  }, []);

  const selected = entities.find((e) => e.id === selectedId) ?? null;

  if (!selected) return <div className="detail-panel"><p>Nothing selected</p></div>;

  return (
    <div className="detail-panel">
      <h3>Detail</h3>
      <p>ID: {selected.id}</p>
      <p>Position: {selected.pos}</p>
      <p>State: {selected.state}</p>
    </div>
  );
}

export function ExamplePanel() {
  const [selectedId, setSelectedId] = useState<string | null>(null);

  return (
    <SelectionContext.Provider value={{ selectedId, setSelectedId }}>
      <div style={{ display: "flex", gap: "16px", padding: "16px" }}>
        <EntityList />
        <DetailPanel />
      </div>
    </SelectionContext.Provider>
  );
}
