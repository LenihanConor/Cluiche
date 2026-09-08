import type { FC } from 'react';
import type { FilterKind } from '../model';

export type ViewKind = 'graph' | 'list' | 'web';

interface FilterDef { key: FilterKind; label: string; cls: string; }

const FILTERS: FilterDef[] = [
    { key: 'all', label: 'All', cls: '' },
    { key: 'broadcast', label: 'Broadcast', cls: 'fb' },
    { key: 'entity', label: 'Entity', cls: 'fe' },
    { key: 'primary', label: 'Primary', cls: 'fp' },
    { key: 'reaction', label: 'Reaction', cls: 'fr' },
    { key: 'dupes', label: 'Dupes', cls: 'fd' },
];

const VIEWS: ViewKind[] = ['graph', 'list', 'web'];

export interface TopbarProps {
    search: string;
    onSearch: (v: string) => void;
    filter: FilterKind;
    onFilter: (f: FilterKind) => void;
    view: ViewKind;
    onView: (v: ViewKind) => void;
}

export const Topbar: FC<TopbarProps> = ({ search, onSearch, filter, onFilter, view, onView }) => (
    <div className="topbar">
        <span className="app-title">DiaMessageBus</span>
        <div className="sep" />
        <span className="subtitle">SCHEMA BROWSER</span>
        <div className="sep" />
        <div className="search-wrap">
            <span className="search-icon">⌕</span>
            <input
                className="search-input"
                type="text"
                placeholder="filter message types…"
                value={search}
                onChange={(e) => onSearch(e.target.value)}
            />
        </div>
        <div className="filters">
            {FILTERS.map((f) => (
                <button
                    key={f.key}
                    className={`f ${f.cls} ${filter === f.key ? 'on' : ''}`.trim()}
                    onClick={() => onFilter(f.key)}
                >
                    {f.label}
                </button>
            ))}
        </div>
        <div className="topbar-right">
            {VIEWS.map((v) => (
                <button
                    key={v}
                    className={`vbtn ${view === v ? 'on' : ''}`.trim()}
                    onClick={() => onView(v)}
                >
                    {v}
                </button>
            ))}
        </div>
    </div>
);
