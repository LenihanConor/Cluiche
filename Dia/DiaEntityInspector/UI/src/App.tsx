import { useEffect, useState, useCallback, CSSProperties } from 'react';
import { theme, ConnectionStatus, TabBar } from '@dia/editor-ui';
import type { Tab } from '@dia/editor-ui';
import { useInspectorStore } from './store';
import { useResizableDivider } from './useResizableDivider';
import { EntityList } from './components/EntityList';
import { ContextStrip } from './components/ContextStrip';
import { FieldsTab } from './components/FieldsTab';
import { QueriesTab } from './components/QueriesTab';
import { MailboxTab } from './components/MailboxTab';
import { WatchTab } from './components/WatchTab';
import { StatusBar } from './components/StatusBar';

export default function App() {
    const [searchText, setSearchText] = useState('');
    const [activeFilter, setActiveFilter] = useState('All');

    const connected = useInspectorStore((s) => s.connected);
    const entities = useInspectorStore((s) => s.entities);
    const queries = useInspectorStore((s) => s.queries);
    const mailbox = useInspectorStore((s) => s.mailbox);
    const watchItems = useInspectorStore((s) => s.watchItems);
    const selectedIdx = useInspectorStore((s) => s.selectedIdx);
    const activeTab = useInspectorStore((s) => s.activeTab);
    const frame = useInspectorStore((s) => s.frame);
    const entityCount = useInspectorStore((s) => s.entityCount);

    const setConnected = useInspectorStore((s) => s.setConnected);
    const setEntities = useInspectorStore((s) => s.setEntities);
    const setQueries = useInspectorStore((s) => s.setQueries);
    const setMailbox = useInspectorStore((s) => s.setMailbox);
    const setWatchItems = useInspectorStore((s) => s.setWatchItems);
    const setSelectedIdx = useInspectorStore((s) => s.setSelectedIdx);
    const setActiveTab = useInspectorStore((s) => s.setActiveTab);
    const setFrame = useInspectorStore((s) => s.setFrame);
    const setEntityCount = useInspectorStore((s) => s.setEntityCount);
    const clearMailbox = useInspectorStore((s) => s.clearMailbox);

    const { leftWidth, dividerProps } = useResizableDivider(180, 400);

    // Bridge wiring — EIRM-003
    useEffect(() => {
        const dispatch = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
                case 'entity_inspector.connection_state':
                    if (d?.connected !== undefined) setConnected(d.connected);
                    break;
                case 'entity_inspector.inspect_data':
                    if (d?.entities) setEntities(d.entities);
                    if (d?.frame !== undefined) setFrame(d.frame);
                    if (d?.entityCount !== undefined) setEntityCount(d.entityCount);
                    break;
                case 'entity_inspector.query_data':
                    if (d?.queries) setQueries(d.queries);
                    break;
                case 'entity_inspector.mailbox_data':
                    // key is `log` (EIRM-003: HTML had `messages`, fixing the silent bug)
                    if (d?.log) setMailbox(d.log);
                    break;
                case 'entity_inspector.watch_data':
                    if (d?.items) setWatchItems(d.items);
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
    }, [setConnected, setEntities, setFrame, setEntityCount, setQueries, setMailbox, setWatchItems]);

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
        sendRequest('entity_inspector.get_connection_state', {});
    }, []);

    const sendBridgeRequest = useCallback((type: string, data: unknown) => {
        if (window.parent && window.parent !== window) {
            window.parent.postMessage({
                __diaFromFrame: true,
                payload: { type, reqId: null, data: data || {} },
            }, '*');
        }
    }, []);

    const handleWatchAdd = useCallback((e: string, c: string, f: string) => {
        sendBridgeRequest('entity_inspector.watch_add', { entity: e, component: c, field: f });
    }, [sendBridgeRequest]);

    const handleWatchRemove = useCallback((index: number) => {
        sendBridgeRequest('entity_inspector.watch_remove', { index });
    }, [sendBridgeRequest]);

    const handleWriteField = useCallback((entityIdx: number, comp: string, field: string, value: string) => {
        sendBridgeRequest('entity_inspector.write_field', { entityIdx, component: comp, field, value });
    }, [sendBridgeRequest]);

    const selectedEntity = selectedIdx !== null ? entities.find((e) => e.i === selectedIdx) ?? null : null;
    const selectedEntityName = selectedEntity?.n ?? null;

    // Tab badge counts
    const fieldCount = selectedEntity?.t.length ?? 0;
    const queryCount = selectedEntity
        ? queries.filter((q) => q.members?.includes(selectedEntity.n)).length
        : 0;
    const mailboxCount = mailbox.length;
    const watchCount = watchItems.length;

    const TABS: Tab[] = [
        { id: 'fields',   label: 'Fields',  count: fieldCount > 0 ? fieldCount : undefined },
        { id: 'queries',  label: 'Queries', count: queryCount > 0 ? queryCount : undefined },
        { id: 'mailbox',  label: 'Mailbox', count: mailboxCount > 0 ? mailboxCount : undefined },
        { id: 'watch',    label: 'Watch',   count: watchCount > 0 ? watchCount : undefined },
    ];

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
                    <div style={{ fontSize: 11, color: theme.textMuted }}>Use the Game Connection panel to connect to a running game</div>
                </div>
            )}

            {/* Title bar */}
            <div style={{ background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, padding: '5px 10px', display: 'flex', alignItems: 'center', gap: 8, flexShrink: 0, userSelect: 'none' }}>
                <ConnectionStatus state={connected ? 'connected' : 'disconnected'} compact />
                <span style={{ fontWeight: 600, color: theme.text }}>DiaEntityInspector</span>
                <span style={{ marginLeft: 'auto', fontSize: 11, color: theme.textMuted }}>
                    {connected && frame > 0 ? `frame #${frame} · ${entityCount} entities` : ''}
                </span>
            </div>

            {/* Main layout */}
            <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
                {/* Left column */}
                <div style={{ width: leftWidth, minWidth: 180, maxWidth: 400, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderRight: `2px solid ${theme.borderMuted}` }}>
                    <EntityList
                        entities={entities}
                        selectedIdx={selectedIdx}
                        onSelect={setSelectedIdx}
                        searchText={searchText}
                        onSearchChange={setSearchText}
                        activeFilter={activeFilter}
                        onFilterChange={setActiveFilter}
                    />
                </div>

                {/* Divider */}
                <div
                    {...dividerProps}
                    style={{ width: 4, background: theme.borderMuted, cursor: 'col-resize', flexShrink: 0 }}
                    data-testid="resize-divider"
                />

                {/* Right column */}
                <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                    {/* Context strip */}
                    <ContextStrip entity={selectedEntity} />

                    {/* Tab bar + content */}
                    {selectedEntity ? (
                        <>
                            <TabBar
                                tabs={TABS}
                                activeTab={activeTab}
                                onTabChange={(id) => setActiveTab(id as any)}
                            />
                            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                                {activeTab === 'fields' && (
                                    <FieldsTab
                                        entity={selectedEntity}
                                        allEntities={entities}
                                        onNavigate={setSelectedIdx}
                                        onWriteField={handleWriteField}
                                    />
                                )}
                                {activeTab === 'queries' && (
                                    <QueriesTab
                                        queries={queries}
                                        selectedEntityName={selectedEntityName}
                                    />
                                )}
                                {activeTab === 'mailbox' && (
                                    <MailboxTab
                                        mailbox={mailbox}
                                        selectedEntityName={selectedEntityName}
                                        onClear={clearMailbox}
                                    />
                                )}
                                {activeTab === 'watch' && (
                                    <WatchTab
                                        watchItems={watchItems}
                                        entities={entities}
                                        onWatchAdd={handleWatchAdd}
                                        onWatchRemove={handleWatchRemove}
                                    />
                                )}
                            </div>
                        </>
                    ) : (
                        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: 6, color: theme.borderMuted }}>
                            <span style={{ fontSize: 28 }}>←</span>
                            <span style={{ fontSize: 11 }}>Select an entity</span>
                        </div>
                    )}
                </div>
            </div>

            {/* Status bar */}
            <StatusBar
                connected={connected}
                frame={frame}
                entityCount={entityCount}
                selectedName={selectedEntityName}
                watchCount={watchCount}
            />
        </div>
    );
}
