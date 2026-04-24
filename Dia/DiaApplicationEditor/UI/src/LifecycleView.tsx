import React, { useMemo, useState } from 'react';
import { useManifestStore } from './ManifestStore';
import type { ManifestData, ProcessingUnitData, PhaseData, ModuleData } from './types';

// ─── Colours ────────────────────────────────────────────────────────────────
const C = {
    active:    '#1565c0',  // module active in this phase
    retained:  '#2e7d32',  // active in both this phase and the next
    released:  '#7b1fa2',  // active here, gone in the next phase
    acquired:  '#e65100',  // not active before, starts here
    inactive:  'transparent',
    border:    '#333',
    header:    '#1e1e1e',
    rowAlt:    '#1a1a1a',
    text:      '#ccc',
    dim:       '#555',
    groupHdr:  '#252526',
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Topological sort of phases using transition graph. Falls back to declaration order.
function sortPhases(phases: PhaseData[], transitions: Array<{ from: string; to: string }>): PhaseData[] {
    const ids = phases.map(p => p.instance_id);
    const inDegree = new Map<string, number>(ids.map(id => [id, 0]));
    const adj = new Map<string, string[]>(ids.map(id => [id, []]));

    for (const t of transitions) {
        if (inDegree.has(t.from) && inDegree.has(t.to)) {
            adj.get(t.from)!.push(t.to);
            inDegree.set(t.to, (inDegree.get(t.to) ?? 0) + 1);
        }
    }

    const queue = ids.filter(id => inDegree.get(id) === 0);
    const sorted: string[] = [];
    while (queue.length > 0) {
        const cur = queue.shift()!;
        sorted.push(cur);
        for (const next of (adj.get(cur) ?? [])) {
            const deg = (inDegree.get(next) ?? 1) - 1;
            inDegree.set(next, deg);
            if (deg === 0) queue.push(next);
        }
    }

    // Any phases not reached (cycle or disconnected) appended at the end
    const seen = new Set(sorted);
    ids.forEach(id => { if (!seen.has(id)) sorted.push(id); });

    const byId = new Map(phases.map(p => [p.instance_id, p]));
    return sorted.map(id => byId.get(id)!).filter(Boolean);
}

type CellState = 'inactive' | 'active' | 'retained' | 'released' | 'acquired';

function classifyCell(
    moduleId: string,
    phaseId: string,
    nextPhaseId: string | null,
    prevPhaseId: string | null,
    modulePhases: Set<string>,
    hasTransitionTo: (from: string, to: string) => boolean,
): CellState {
    const here = modulePhases.has(phaseId);
    if (!here) return 'inactive';

    const goingNext = nextPhaseId !== null && hasTransitionTo(phaseId, nextPhaseId);
    const cameFromPrev = prevPhaseId !== null && hasTransitionTo(prevPhaseId, phaseId);

    const activeNext = nextPhaseId !== null && modulePhases.has(nextPhaseId) && goingNext;
    const activePrev = prevPhaseId !== null && modulePhases.has(prevPhaseId) && cameFromPrev;

    if (activeNext && activePrev) return 'retained';
    if (activeNext && !activePrev) return 'acquired';
    if (!activeNext && activePrev) return 'released';
    // Active here, no adjacent transition context — just show active
    return 'active';
}

const CELL_LABEL: Record<CellState, string> = {
    inactive: '',
    active:   '●',
    retained: '↔',
    released: '↓',
    acquired: '↑',
};

const CELL_TITLE: Record<CellState, string> = {
    inactive: 'Not active',
    active:   'Active',
    retained: 'Retained across transition',
    released: 'Released on transition',
    acquired: 'Acquired on transition',
};

// ─── Per-PU grid ──────────────────────────────────────────────────────────────

interface PUGridProps {
    pu: ProcessingUnitData;
    selectedNode: string | null;
    onSelectModule: (nodeId: string) => void;
}

const PUGrid: React.FC<PUGridProps> = ({ pu, selectedNode, onSelectModule }) => {
    const sortedPhases = useMemo(
        () => sortPhases(pu.phases, pu.transitions),
        [pu.phases, pu.transitions],
    );

    const transitionSet = useMemo(() => {
        const s = new Set<string>();
        for (const t of pu.transitions) s.add(`${t.from}__${t.to}`);
        return s;
    }, [pu.transitions]);

    const hasTransitionTo = (from: string, to: string) => transitionSet.has(`${from}__${to}`);

    // All modules, sorted alphabetically for stable row order
    const modules = useMemo(
        () => [...pu.modules].sort((a, b) => a.instance_id.localeCompare(b.instance_id)),
        [pu.modules],
    );

    if (modules.length === 0 || sortedPhases.length === 0) return null;

    const COL_W = Math.max(80, Math.min(140, Math.floor(560 / sortedPhases.length)));
    const ROW_H = 28;
    const LABEL_W = 200;

    return (
        <div style={{ marginBottom: 24 }}>
            {/* PU header */}
            <div style={{ background: C.groupHdr, padding: '4px 8px', fontSize: 11, color: '#888', borderBottom: `1px solid ${C.border}`, fontWeight: 600, letterSpacing: '0.05em' }}>
                {pu.instance_id}
            </div>

            <div style={{ overflowX: 'auto' }}>
                <table style={{ borderCollapse: 'collapse', tableLayout: 'fixed', width: LABEL_W + COL_W * sortedPhases.length }}>
                    {/* Column headers — phase names */}
                    <thead>
                        <tr>
                            <th style={{ width: LABEL_W, minWidth: LABEL_W, background: C.header, borderBottom: `1px solid ${C.border}`, borderRight: `1px solid ${C.border}`, padding: '4px 8px', textAlign: 'left', fontSize: 11, color: C.dim, fontWeight: 400 }}>
                                Module
                            </th>
                            {sortedPhases.map(phase => (
                                <th
                                    key={phase.instance_id}
                                    style={{ width: COL_W, background: C.header, borderBottom: `1px solid ${C.border}`, borderRight: `1px solid ${C.border}`, padding: '3px 4px', textAlign: 'center', fontSize: 10, color: C.text, fontWeight: 400, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}
                                    title={phase.instance_id}
                                >
                                    {phase.instance_id}
                                </th>
                            ))}
                        </tr>
                    </thead>

                    <tbody>
                        {modules.map((mod, rowIdx) => {
                            const nodeId = `${pu.instance_id}_${/* phase context not needed for module row */''}_${mod.instance_id}`;
                            const isSelected = selectedNode?.endsWith(`_${mod.instance_id}`) ?? false;
                            const modulePhases = new Set(mod.phases ?? []);

                            return (
                                <tr
                                    key={mod.instance_id}
                                    style={{ background: isSelected ? '#094771' : rowIdx % 2 === 0 ? 'transparent' : C.rowAlt, cursor: 'pointer' }}
                                    onClick={() => onSelectModule(nodeId)}
                                >
                                    <td style={{ padding: '3px 8px', fontSize: 11, color: isSelected ? '#fff' : C.text, borderRight: `1px solid ${C.border}`, borderBottom: `1px solid #222`, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }} title={`${mod.instance_id} (${mod.type})`}>
                                        <span style={{ color: C.dim, marginRight: 4 }}>⚙</span>
                                        {mod.instance_id}
                                        <span style={{ color: C.dim, fontSize: 10, marginLeft: 4 }}>({mod.type})</span>
                                    </td>
                                    {sortedPhases.map((phase, colIdx) => {
                                        const prevPhase = colIdx > 0 ? sortedPhases[colIdx - 1].instance_id : null;
                                        const nextPhase = colIdx < sortedPhases.length - 1 ? sortedPhases[colIdx + 1].instance_id : null;
                                        const state = classifyCell(mod.instance_id, phase.instance_id, nextPhase, prevPhase, modulePhases, hasTransitionTo);
                                        const bg = state === 'inactive' ? 'transparent' : C[state];

                                        return (
                                            <td
                                                key={phase.instance_id}
                                                title={CELL_TITLE[state]}
                                                style={{ width: COL_W, background: bg, borderRight: `1px solid ${C.border}`, borderBottom: `1px solid #222`, textAlign: 'center', fontSize: 12, color: '#fff', userSelect: 'none' }}
                                            >
                                                {CELL_LABEL[state]}
                                            </td>
                                        );
                                    })}
                                </tr>
                            );
                        })}
                    </tbody>
                </table>
            </div>
        </div>
    );
};

// ─── Legend ───────────────────────────────────────────────────────────────────

const Legend: React.FC = () => (
    <div style={{ display: 'flex', gap: 16, padding: '6px 12px', background: C.header, borderBottom: `1px solid ${C.border}`, flexShrink: 0, flexWrap: 'wrap' }}>
        {([
            ['active',   C.active,   '● Active'],
            ['retained', C.retained, '↔ Retained across transition'],
            ['released', C.released, '↓ Released on transition'],
            ['acquired', C.acquired, '↑ Acquired on transition'],
        ] as const).map(([, color, label]) => (
            <span key={label} style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11, color: C.text }}>
                <span style={{ width: 12, height: 12, background: color, display: 'inline-block', borderRadius: 2 }} />
                {label}
            </span>
        ))}
    </div>
);

// ─── Root view ────────────────────────────────────────────────────────────────

export const LifecycleView: React.FC = () => {
    const manifest     = useManifestStore(s => s.manifest) as ManifestData | null;
    const selectedNode = useManifestStore(s => s.selectedNode);
    const setSelected  = useManifestStore(s => s.setSelectedNode);

    const [filter, setFilter] = useState('');

    const pus = useMemo(() => {
        if (!manifest) return [];
        if (!filter) return manifest.processing_units;
        const lower = filter.toLowerCase();
        return manifest.processing_units.map(pu => ({
            ...pu,
            modules: pu.modules.filter(m =>
                m.instance_id.toLowerCase().includes(lower) ||
                m.type.toLowerCase().includes(lower)
            ),
        })).filter(pu => pu.modules.length > 0);
    }, [manifest, filter]);

    if (!manifest) return null;

    const handleSelectModule = (nodeId: string) => {
        setSelected(nodeId);
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden', background: '#1e1e1e' }}>
            <Legend />
            <div style={{ padding: '4px 8px', borderBottom: `1px solid ${C.border}`, flexShrink: 0 }}>
                <input
                    type="text"
                    placeholder="Filter modules..."
                    value={filter}
                    onChange={e => setFilter(e.target.value)}
                    style={{ width: '100%', background: '#2a2a2a', border: `1px solid #444`, color: C.text, padding: '3px 6px', fontSize: 12, borderRadius: 3 }}
                />
            </div>
            <div style={{ flex: 1, overflowY: 'auto', overflowX: 'auto' }}>
                {pus.map(pu => (
                    <PUGrid
                        key={pu.instance_id}
                        pu={pu as ProcessingUnitData}
                        selectedNode={selectedNode}
                        onSelectModule={handleSelectModule}
                    />
                ))}
            </div>
        </div>
    );
};
