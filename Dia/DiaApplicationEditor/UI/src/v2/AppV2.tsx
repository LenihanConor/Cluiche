import React, { useState, useEffect } from 'react';

type Tab = 'graph' | 'presence' | 'streams';

export const AppV2: React.FC = () => {
    const [activeTab, setActiveTab] = useState<Tab>('graph');

    useEffect(() => {
        (window as any).DiaEditor_onDataChanged = (topic: string, data: unknown) => {
            // dispatch to stores based on topic
            console.log('[DiaEditor] onDataChanged', topic, data);
        };
    }, []);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100vh', background: '#1e1e1e', color: '#ccc', fontFamily: 'sans-serif' }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', height: 40, background: '#2d2d2d', borderBottom: '1px solid #444', padding: '0 8px', gap: 8 }}>
                <span style={{ fontWeight: 600, fontSize: 13 }}>Application Flow Editor</span>
                <span style={{ flex: 1 }} />
                {/* LiveConnectionButton placeholder */}
                <span id="live-connection-slot" />
            </div>

            {/* Tab bar */}
            <div style={{ display: 'flex', background: '#252526', borderBottom: '1px solid #444' }}>
                {(['graph', 'presence', 'streams'] as Tab[]).map(tab => (
                    <button
                        key={tab}
                        onClick={() => setActiveTab(tab)}
                        style={{
                            padding: '6px 16px',
                            background: activeTab === tab ? '#1e1e1e' : 'transparent',
                            border: 'none',
                            borderBottom: activeTab === tab ? '2px solid #007acc' : '2px solid transparent',
                            color: activeTab === tab ? '#fff' : '#ccc',
                            cursor: 'pointer',
                            fontSize: 12,
                            textTransform: 'capitalize',
                        }}
                    >
                        {tab === 'graph' ? 'Graph' : tab === 'presence' ? 'Presence' : 'Streams'}
                    </button>
                ))}
            </div>

            {/* Main content + sidebar */}
            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
                {/* Tab content */}
                <div style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
                    {activeTab === 'graph' && <div id="graph-tab-content" style={{ height: '100%' }}>Graph view (coming soon)</div>}
                    {activeTab === 'presence' && <div id="presence-tab-content" style={{ height: '100%' }}>Presence grid (coming soon)</div>}
                    {activeTab === 'streams' && <div id="streams-tab-content" style={{ height: '100%' }}>Streams tab (coming soon)</div>}
                </div>

                {/* Sidebar */}
                <div id="sidebar-container" style={{ width: 280, borderLeft: '1px solid #444', overflow: 'auto', background: '#252526' }}>
                    <div style={{ padding: 12, color: '#888', fontSize: 12 }}>Select a node to inspect</div>
                </div>
            </div>

            {/* Footer: ValidationBar placeholder */}
            <div id="validation-bar-slot" style={{ height: 28, background: '#007acc', display: 'flex', alignItems: 'center', padding: '0 8px', fontSize: 11 }}>
                No manifest loaded
            </div>
        </div>
    );
};
