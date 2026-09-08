import { useState, useRef, useCallback, useEffect, CSSProperties, JSX } from 'react';
import { useAssetRuntimeStore } from '../store';
import type { AssetRow, AssetState } from '../types';

// ─── Constants ────────────────────────────────────────────────────────────────

const ROW_HEIGHT = 24;
const BUFFER_ROWS = 5;

const STATE_COLORS: Record<AssetState, string> = {
    Loaded:   '#4ec9a0',
    Loading:  '#dcdcaa',
    Staged:   '#808080',
    Failed:   '#f44747',
    Unloaded: '#ce9178',
    Null:     '#555555',
};

const STATE_OPTIONS: string[] = ['All', 'Null', 'Staged', 'Loading', 'Loaded', 'Failed', 'Unloaded'];

type SortKey = keyof AssetRow | null;
type SortDir = 'asc' | 'desc';

// ─── Bridge helper ────────────────────────────────────────────────────────────

function sendBridgeRequest(type: string, data: unknown) {
    window.parent.postMessage(
        { __diaFromFrame: true, payload: { type, reqId: null, data } },
        '*',
    );
}

// ─── Component ────────────────────────────────────────────────────────────────

export function AssetStateTable(): JSX.Element {
    const assets         = useAssetRuntimeStore((s) => s.assets);
    const total          = useAssetRuntimeStore((s) => s.total);
    const storeFilter    = useAssetRuntimeStore((s) => s.stateFilter);
    const storeIdSearch  = useAssetRuntimeStore((s) => s.idSearch);
    const setTableFilters = useAssetRuntimeStore((s) => s.setTableFilters);

    // Local filter state (initialized from store — restored from session)
    const [stateFilter, setStateFilter] = useState<string>(() => storeFilter || 'All');
    const [idSearch, setIdSearch]       = useState<string>(() => storeIdSearch || '');

    // Keep local in sync when store updates externally (e.g. bridge push)
    useEffect(() => {
        if (storeFilter !== '' && storeFilter !== stateFilter) {
            setStateFilter(storeFilter);
        }
    }, [storeFilter]); // eslint-disable-line react-hooks/exhaustive-deps

    useEffect(() => {
        if (storeIdSearch !== idSearch) {
            setIdSearch(storeIdSearch);
        }
    }, [storeIdSearch]); // eslint-disable-line react-hooks/exhaustive-deps

    // Sort state
    const [sortKey, setSortKey] = useState<SortKey>(null);
    const [sortDir, setSortDir] = useState<SortDir>('asc');

    // Selection
    const [selectedId, setSelectedId] = useState<string | null>(null);

    // Poll interval
    const [pollInterval, setPollInterval] = useState<number>(1.0);

    // Last refresh timestamp
    const [lastRefresh, setLastRefresh] = useState<string>('');

    // Scroll state for virtual scrolling
    const [scrollTop, setScrollTop]         = useState(0);
    const [containerHeight, setContainerHeight] = useState(400);
    const containerRef = useRef<HTMLDivElement>(null);
    const rafRef = useRef<number | null>(null);

    // Track snapshot updates for last-refresh time
    useEffect(() => {
        if (assets.length > 0 || total > 0) {
            const now = new Date();
            const h = String(now.getHours()).padStart(2, '0');
            const m = String(now.getMinutes()).padStart(2, '0');
            const s = String(now.getSeconds()).padStart(2, '0');
            setLastRefresh(`${h}:${m}:${s}`);
        }
    }, [assets, total]);

    // Measure container height (ResizeObserver not available in jsdom — falls back to clientHeight)
    useEffect(() => {
        const el = containerRef.current;
        if (!el) return;
        setContainerHeight(el.clientHeight);
        if (typeof ResizeObserver === 'undefined') return;
        const ro = new ResizeObserver(() => {
            setContainerHeight(el.clientHeight);
        });
        ro.observe(el);
        return () => ro.disconnect();
    }, []);

    // ─── Filtering ───────────────────────────────────────────────────────────

    const filtered: AssetRow[] = assets.filter((row) => {
        const matchState =
            !stateFilter || stateFilter === 'All' || row.state === stateFilter;
        const matchId =
            !idSearch || row.id.toLowerCase().includes(idSearch.toLowerCase());
        return matchState && matchId;
    });

    // ─── Sorting ─────────────────────────────────────────────────────────────

    const sorted: AssetRow[] = sortKey
        ? [...filtered].sort((a, b) => {
              const av = a[sortKey];
              const bv = b[sortKey];
              let cmp = 0;
              if (typeof av === 'number' && typeof bv === 'number') {
                  cmp = av - bv;
              } else {
                  cmp = String(av).localeCompare(String(bv));
              }
              return sortDir === 'asc' ? cmp : -cmp;
          })
        : filtered;

    // ─── Virtual scrolling ───────────────────────────────────────────────────

    const totalRows    = sorted.length;
    const totalHeight  = totalRows * ROW_HEIGHT;

    const firstVisible = Math.floor(scrollTop / ROW_HEIGHT);
    const visibleCount = Math.ceil(containerHeight / ROW_HEIGHT);
    const startIdx     = Math.max(0, firstVisible - BUFFER_ROWS);
    const endIdx       = Math.min(totalRows, firstVisible + visibleCount + BUFFER_ROWS);

    const paddingTop    = startIdx * ROW_HEIGHT;
    const paddingBottom = Math.max(0, (totalRows - endIdx) * ROW_HEIGHT);

    const visibleRows = sorted.slice(startIdx, endIdx);

    // ─── Handlers ────────────────────────────────────────────────────────────

    const handleScroll = useCallback(() => {
        if (rafRef.current !== null) return;
        rafRef.current = requestAnimationFrame(() => {
            if (containerRef.current) {
                setScrollTop(containerRef.current.scrollTop);
            }
            rafRef.current = null;
        });
    }, []);

    const handleStateFilterChange = useCallback((value: string) => {
        setStateFilter(value);
        const sf = value === 'All' ? '' : value;
        setTableFilters(sf, idSearch);
        sendBridgeRequest('asset_runtime_inspector.update_filters', { stateFilter: sf, idSearch });
    }, [idSearch, setTableFilters]);

    const handleIdSearchChange = useCallback((value: string) => {
        setIdSearch(value);
        const sf = stateFilter === 'All' ? '' : stateFilter;
        setTableFilters(sf, value);
        sendBridgeRequest('asset_runtime_inspector.update_filters', { stateFilter: sf, idSearch: value });
    }, [stateFilter, setTableFilters]);

    const handleRowClick = useCallback((row: AssetRow) => {
        setSelectedId(row.id);
        sendBridgeRequest('asset_runtime_inspector.select_asset', { assetId: row.id });
    }, []);

    const handleColumnSort = useCallback((key: SortKey) => {
        if (sortKey === key) {
            setSortDir((d) => (d === 'asc' ? 'desc' : 'asc'));
        } else {
            setSortKey(key);
            setSortDir('asc');
        }
    }, [sortKey]);

    const handleRefresh = useCallback(() => {
        sendBridgeRequest('asset_runtime_inspector.force_refresh', {});
    }, []);

    const handlePollIntervalCommit = useCallback((value: number) => {
        const clamped = Math.max(0.1, value);
        setPollInterval(clamped);
        sendBridgeRequest('asset_runtime_inspector.set_poll_interval', { interval: clamped });
    }, []);

    // ─── Column header helper ─────────────────────────────────────────────────

    const colHeader = (label: string, key: SortKey) => {
        const indicator = sortKey === key ? (sortDir === 'asc' ? ' ▲' : ' ▼') : '';
        return (
            <div
                key={label}
                role="columnheader"
                style={colHeaderStyle}
                onClick={() => handleColumnSort(key)}
                data-sort-key={key}
            >
                {label}{indicator}
            </div>
        );
    };

    // ─── Styles ───────────────────────────────────────────────────────────────

    const containerStyle: CSSProperties = {
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        overflow: 'hidden',
        fontFamily: "'Consolas', 'Courier New', monospace",
        fontSize: 11,
        color: '#d4d4d4',
        background: '#1e1e1e',
    };

    const controlsStyle: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 8,
        padding: '4px 8px',
        background: '#252526',
        borderBottom: '1px solid #3c3c3c',
        flexShrink: 0,
    };

    const inputStyle: CSSProperties = {
        background: '#2d2d2d',
        border: '1px solid #3c3c3c',
        color: '#d4d4d4',
        padding: '2px 4px',
        fontSize: 11,
        borderRadius: 2,
        outline: 'none',
    };

    const selectStyle: CSSProperties = {
        ...inputStyle,
        cursor: 'pointer',
    };

    const buttonStyle: CSSProperties = {
        background: '#0e639c',
        border: 'none',
        color: '#fff',
        padding: '2px 8px',
        fontSize: 11,
        borderRadius: 2,
        cursor: 'pointer',
    };

    const tableHeaderStyle: CSSProperties = {
        display: 'flex',
        background: '#2d2d2d',
        borderBottom: '1px solid #3c3c3c',
        flexShrink: 0,
    };

    const colHeaderStyle: CSSProperties = {
        padding: '3px 6px',
        fontWeight: 600,
        cursor: 'pointer',
        userSelect: 'none',
        flex: 1,
        whiteSpace: 'nowrap',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        color: '#c8c8c8',
    };

    const scrollAreaStyle: CSSProperties = {
        flex: 1,
        overflowY: 'auto',
        position: 'relative',
    };

    const statusBarStyle: CSSProperties = {
        display: 'flex',
        alignItems: 'center',
        gap: 12,
        padding: '3px 8px',
        background: '#252526',
        borderTop: '1px solid #3c3c3c',
        flexShrink: 0,
        color: '#888',
        fontSize: 11,
    };

    // ─── Render ───────────────────────────────────────────────────────────────

    return (
        <div style={containerStyle} data-testid="asset-state-table">
            {/* Controls bar */}
            <div style={controlsStyle} data-testid="controls-bar">
                <label style={{ color: '#888' }}>State:</label>
                <select
                    style={selectStyle}
                    value={stateFilter}
                    onChange={(e) => handleStateFilterChange(e.target.value)}
                    data-testid="state-filter"
                    aria-label="State filter"
                >
                    {STATE_OPTIONS.map((opt) => (
                        <option key={opt} value={opt}>{opt}</option>
                    ))}
                </select>

                <label style={{ color: '#888' }}>ID:</label>
                <input
                    type="text"
                    style={{ ...inputStyle, minWidth: 120 }}
                    value={idSearch}
                    onChange={(e) => handleIdSearchChange(e.target.value)}
                    placeholder="search id…"
                    data-testid="id-search"
                    aria-label="ID search"
                />

                <label style={{ color: '#888' }}>Poll (s):</label>
                <input
                    type="number"
                    style={{ ...inputStyle, width: 60 }}
                    min={0.1}
                    step={0.1}
                    value={pollInterval}
                    onChange={(e) => setPollInterval(parseFloat(e.target.value) || 0.1)}
                    onBlur={(e) => handlePollIntervalCommit(parseFloat(e.target.value) || 0.1)}
                    data-testid="poll-interval"
                    aria-label="Poll interval"
                />

                <button
                    style={buttonStyle}
                    onClick={handleRefresh}
                    data-testid="refresh-button"
                >
                    Refresh
                </button>
            </div>

            {/* Column headers */}
            <div style={tableHeaderStyle} role="row" data-testid="table-header">
                {colHeader('id',         'id')}
                {colHeader('state',      'state')}
                {colHeader('scope',      'scope')}
                {colHeader('refCount',   'refCount')}
                {colHeader('deployPath', 'deployPath')}
            </div>

            {/* Virtual scroll area */}
            <div
                ref={containerRef}
                style={scrollAreaStyle}
                onScroll={handleScroll}
                data-testid="scroll-area"
            >
                <div style={{ height: totalHeight, position: 'relative' }}>
                    {/* Top spacer */}
                    {paddingTop > 0 && (
                        <div style={{ height: paddingTop }} data-testid="spacer-top" />
                    )}

                    {/* Rendered rows */}
                    {visibleRows.map((row) => {
                        const isSelected = row.id === selectedId;
                        const rowStyle: CSSProperties = {
                            display: 'flex',
                            height: ROW_HEIGHT,
                            alignItems: 'center',
                            background: isSelected ? '#04395e' : 'transparent',
                            cursor: 'pointer',
                            borderBottom: '1px solid #2a2a2a',
                        };
                        return (
                            <div
                                key={row.id}
                                style={rowStyle}
                                onClick={() => handleRowClick(row)}
                                data-testid={`row-${row.id}`}
                                role="row"
                            >
                                <div style={{ flex: 1, padding: '0 6px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                    {row.id}
                                </div>
                                <div style={{ flex: 1, padding: '0 6px', display: 'flex', alignItems: 'center', gap: 4 }}>
                                    <span style={{
                                        width: 8, height: 8, borderRadius: '50%',
                                        background: STATE_COLORS[row.state],
                                        display: 'inline-block', flexShrink: 0,
                                    }} />
                                    {row.state}
                                </div>
                                <div style={{ flex: 1, padding: '0 6px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                    {row.scope}
                                </div>
                                <div style={{ flex: 1, padding: '0 6px' }}>
                                    {row.refCount}
                                </div>
                                <div style={{ flex: 2, padding: '0 6px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                    {row.deployPath}
                                </div>
                            </div>
                        );
                    })}

                    {/* Bottom spacer */}
                    {paddingBottom > 0 && (
                        <div style={{ height: paddingBottom }} data-testid="spacer-bottom" />
                    )}
                </div>
            </div>

            {/* Status bar */}
            <div style={statusBarStyle} data-testid="status-bar">
                <span data-testid="asset-count">{filtered.length} / {total} assets</span>
                {lastRefresh && (
                    <span data-testid="last-refresh">Last refresh: {lastRefresh}</span>
                )}
            </div>
        </div>
    );
}
