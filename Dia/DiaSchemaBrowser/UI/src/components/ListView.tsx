import type { FC } from 'react';
import type { Message } from '../model';
import { producerIsAdapter, consumerIsComponent } from '../model';

export interface ListViewProps {
    message: Message | undefined;
    active: boolean;
}

export const ListView: FC<ListViewProps> = ({ message, active }) => {
    if (!message) {
        return (
            <div className={`list-view ${active ? 'on' : ''}`.trim()}>
                <div className="py-empty">Select a message type from the left panel.</div>
            </div>
        );
    }

    const dupeLabel = message.dupes.length ? `⚠ potential dupe: ${message.dupes.join(', ')}` : '';

    return (
        <div className={`list-view ${active ? 'on' : ''}`.trim()}>
            <div className="lv-title">{message.id}</div>
            <div className="lv-meta">
                <span className={`meta-pill ${message.router}`}>{message.router} router</span>
                <span className={`meta-pill ${message.pass}`}>{message.pass} pass</span>
                {dupeLabel && (
                    <span
                        className="meta-pill"
                        style={{
                            color: 'var(--dupe)',
                            border: '1px solid rgba(240,96,112,.3)',
                            background: 'rgba(240,96,112,.07)',
                        }}
                    >
                        {dupeLabel}
                    </span>
                )}
            </div>

            <div className="lv-section">
                <div className="lv-sl">Producers</div>
                {message.producers.length === 0 && <div className="lv-empty">— none declared —</div>}
                {message.producers.map((p) => {
                    const isA = producerIsAdapter(p);
                    return (
                        <div className="lv-row" key={p}>
                            <span className="lv-node">{p}</span>
                            <span className={`lv-node-type ${isA ? 'a' : 'p'}`}>{isA ? 'adapter' : 'system'}</span>
                        </div>
                    );
                })}
            </div>

            <div className="lv-divider">
                <div className="lv-div-line" />
                <span className={`lv-div-label ${message.pass === 'reaction' ? 'r' : ''}`.trim()}>
                    → {message.id} · {message.pass} pass →
                </span>
                <div className="lv-div-line" />
            </div>

            <div className="lv-section">
                <div className="lv-sl">Consumers</div>
                {message.consumers.length === 0 && <div className="lv-empty">— none declared —</div>}
                {message.consumers.map((c) => {
                    const isComp = consumerIsComponent(message.router, c);
                    return (
                        <div className="lv-row" key={c}>
                            <span className="lv-node">{c}</span>
                            <span className={`lv-node-type ${isComp ? 'e' : 'b'}`}>{isComp ? 'component' : 'system'}</span>
                        </div>
                    );
                })}
            </div>
        </div>
    );
};
