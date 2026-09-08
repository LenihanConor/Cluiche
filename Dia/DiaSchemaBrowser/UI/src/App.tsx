import { useEffect, useMemo, useState } from 'react';
import type { FC } from 'react';
import { useBridgeRequest } from '@dia/editor-ui';
import {
    buildModel, visibleMessages,
    type ScanResult, type SchemaModel, type FilterKind,
} from './model';
import { Topbar, type ViewKind } from './components/Topbar';
import { MessageList } from './components/MessageList';
import { GraphView } from './components/GraphView';
import { ListView } from './components/ListView';
import { WebView } from './components/WebView';
import { PayloadTab } from './components/PayloadTab';
import { AnalysisTab } from './components/AnalysisTab';

type TabKind = 'payload' | 'analysis';
type LoadState = 'loading' | 'ready' | 'error';

export const App: FC = () => {
    const request = useBridgeRequest<ScanResult>();

    const [model, setModel] = useState<SchemaModel | null>(null);
    const [loadState, setLoadState] = useState<LoadState>('loading');
    const [errorMsg, setErrorMsg] = useState('');

    const [selected, setSelected] = useState<string | null>(null);
    const [view, setView] = useState<ViewKind>('graph');
    const [tab, setTab] = useState<TabKind>('payload');
    const [filter, setFilter] = useState<FilterKind>('all');
    const [search, setSearch] = useState('');

    useEffect(() => {
        let cancelled = false;
        request('schema.scan')
            .then((scan) => {
                if (cancelled) return;
                const m = buildModel(scan);
                setModel(m);
                setLoadState('ready');
                if (m.messages.length > 0) setSelected(m.messages[0].id);
            })
            .catch((err: unknown) => {
                if (cancelled) return;
                setErrorMsg(err instanceof Error ? err.message : String(err));
                setLoadState('error');
            });
        return () => {
            cancelled = true;
        };
    }, [request]);

    const visible = useMemo(
        () => (model ? visibleMessages(model.messages, filter, search) : []),
        [model, filter, search],
    );

    const selectedMsg = useMemo(
        () => (model && selected ? model.byId[selected] : undefined),
        [model, selected],
    );

    if (loadState === 'loading') {
        return (
            <>
                <Topbar
                    search={search} onSearch={setSearch}
                    filter={filter} onFilter={setFilter}
                    view={view} onView={setView}
                />
                <div className="sb-status">Scanning for .diagamemessages declarations…</div>
            </>
        );
    }

    if (loadState === 'error' || !model) {
        return (
            <>
                <Topbar
                    search={search} onSearch={setSearch}
                    filter={filter} onFilter={setFilter}
                    view={view} onView={setView}
                />
                <div className="sb-status">
                    Could not load schema declarations.
                    <br />
                    {errorMsg}
                </div>
            </>
        );
    }

    const dupeCount = model.dupePairs.length;

    return (
        <>
            <Topbar
                search={search} onSearch={setSearch}
                filter={filter} onFilter={setFilter}
                view={view} onView={setView}
            />
            <div className="workspace">
                <div className="workspace-main">
                    <MessageList messages={visible} selected={selected} onSelect={setSelected} />

                    <div className="panel-center">
                        <GraphView message={selectedMsg} active={view === 'graph'} />
                        <ListView message={selectedMsg} active={view === 'list'} />
                        <WebView
                            messages={model.messages}
                            active={view === 'web'}
                            onSelectMessage={setSelected}
                        />
                    </div>
                </div>

                <div className="panel-bottom">
                    <div className="tab-bar">
                        <button
                            className={`tbtn ${tab === 'payload' ? 'on' : ''}`.trim()}
                            onClick={() => setTab('payload')}
                        >
                            Payload
                        </button>
                        <button
                            className={`tbtn ${tab === 'analysis' ? 'on' : ''}`.trim()}
                            onClick={() => setTab('analysis')}
                        >
                            Schema Analysis <span className="wn">{dupeCount}</span>
                        </button>
                    </div>
                    <div className="tab-content">
                        <PayloadTab message={selectedMsg} active={tab === 'payload'} />
                        <AnalysisTab message={selectedMsg} model={model} active={tab === 'analysis'} />
                    </div>
                </div>
            </div>
        </>
    );
};
