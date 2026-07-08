import { useEffect, useState, CSSProperties } from 'react';
import { theme, ConnectionStatus, EmptyState } from '@dia/editor-ui';
import { useBlackboardInspectorStore } from './store';
import { BoardCard } from './components/BoardCard';

export default function App() {
    const connected = useBlackboardInspectorStore((s) => s.connected);
    const boards = useBlackboardInspectorStore((s) => s.boards);
    const filterText = useBlackboardInspectorStore((s) => s.filterText);

    const setConnected = useBlackboardInspectorStore((s) => s.setConnected);
    const setBoards = useBlackboardInspectorStore((s) => s.setBoards);
    const setFilterText = useBlackboardInspectorStore((s) => s.setFilterText);

    const [allExpanded, setAllExpanded] = useState<boolean | null>(null);

    // Bridge wiring — EIRM-003
    useEffect(() => {
        const dispatch = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
                case 'blackboard_inspector.connection_state':
                    if (d?.connected !== undefined) setConnected(d.connected);
                    break;
                case 'blackboard_inspector.state':
                    if (Array.isArray(d?.boards)) setBoards(d.boards);
                    break;
            }
        };

        (window as any).DiaEditor_onDataChanged = (msg: { topic: string; data: unknown }) => {
            dispatch(msg.topic, msg.data);
        };

        const onMessage = (e: MessageEvent) => {
            const env = e.data;
            if (env && env.__dia === true && typeof env.topic === 'string') {
                dispatch(env.topic, env.data);
            }
        };
        window.addEventListener('message', onMessage);
        return () => window.removeEventListener('message', onMessage);
    }, [setConnected, setBoards]);

    // Init request for connection state
    useEffect(() => {
        const sendRequest = (type: string, data: unknown) => {
            if (window.parent && window.parent !== window) {
                window.parent.postMessage({
                    __diaFromFrame: true,
                    payload: { type, reqId: null, data: data || {} },
                }, '*');
            }
        };
        sendRequest('blackboard_inspector.get_connection_state', {});
    }, []);

    const filteredBoards = boards.filter((board) => {
        if (!filterText.trim()) return true;
        const query = filterText.trim().toLowerCase();
        return (
            board.label.toLowerCase().includes(query) ||
            board.id.toLowerCase().includes(query)
        );
    });

    const rootStyle: CSSProperties = {
        fontFamily: "'Segoe UI', system-ui, sans-serif",
        background: theme.bg,
        color: theme.text,
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        position: 'relative',
        overflow: 'hidden',
        fontSize: 12,
    };

    const overlayStyle: CSSProperties = {
        position: 'absolute',
        top: 0, left: 0, right: 0, bottom: 0,
        background: 'rgba(0,0,0,0.6)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 100,
        flexDirection: 'column',
        gap: 8,
    };

    return (
        <div style={rootStyle}>
            {/* Disconnect overlay */}
            {!connected && (
                <div style={overlayStyle} data-testid="disconnect-overlay">
                    <div style={{ fontSize: 14, color: '#f44747' }}>No game connected</div>
                    <div style={{ fontSize: 11, color: theme.textMuted }}>
                        Use the Game Connection panel in the toolbar to connect.
                    </div>
                </div>
            )}

            {/* Title bar */}
            <div style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, padding: '5px 10px', display: 'flex', alignItems: 'center', gap: 8, flexShrink: 0, userSelect: 'none' }}>
                <ConnectionStatus state={connected ? 'connected' : 'disconnected'} compact />
                <span style={{ fontWeight: 600, color: theme.text }}>Blackboard Inspector</span>
            </div>

            {/* Toolbar */}
            <div style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, padding: '4px 8px', display: 'flex', alignItems: 'center', gap: 4, flexShrink: 0 }}>
                <input
                    type="search"
                    placeholder="Filter boards…"
                    value={filterText}
                    onChange={(e) => setFilterText(e.target.value)}
                    style={{
                        flex: 1,
                        background: '#2d2d2d',
                        border: `1px solid ${theme.border}`,
                        borderRadius: 3,
                        color: theme.text,
                        fontSize: 11,
                        padding: '3px 7px',
                        outline: 'none',
                        fontFamily: "'Segoe UI', system-ui, sans-serif",
                    }}
                />
                <button
                    title="Expand all"
                    onClick={() => setAllExpanded(true)}
                    style={{ background: '#3a3d41', border: `1px solid ${theme.border}`, borderRadius: 3, color: theme.textMuted, cursor: 'pointer', fontSize: 11, padding: '2px 7px', lineHeight: '16px', flexShrink: 0 }}
                >
                    ⊞
                </button>
                <button
                    title="Collapse all"
                    onClick={() => setAllExpanded(false)}
                    style={{ background: '#3a3d41', border: `1px solid ${theme.border}`, borderRadius: 3, color: theme.textMuted, cursor: 'pointer', fontSize: 11, padding: '2px 7px', lineHeight: '16px', flexShrink: 0 }}
                >
                    ⊟
                </button>
            </div>

            {/* Scroll area */}
            <div style={{ flex: 1, overflowY: 'auto', padding: 6 }}>
                {boards.length === 0 ? (
                    <EmptyState message="No blackboards registered" />
                ) : filteredBoards.length === 0 ? (
                    <EmptyState message={`No boards match "${filterText}"`} />
                ) : (
                    filteredBoards.map((board) => (
                        <BoardCard key={board.id} board={board} expandOverride={allExpanded} />
                    ))
                )}
            </div>
        </div>
    );
}
