import type { FC } from 'react';
import type { Message } from '../model';

export interface MessageListProps {
    messages: Message[];
    selected: string | null;
    onSelect: (id: string) => void;
}

export const MessageList: FC<MessageListProps> = ({ messages, selected, onSelect }) => (
    <div className="panel-left">
        <div className="plh">
            <span className="pll">Message Types</span>
            <span className="cnt">{messages.length}</span>
        </div>
        <div className="msg-list">
            {messages.map((m) => {
                const hasDupe = m.dupes.length > 0;
                const isSel = selected === m.id;
                return (
                    <div
                        key={m.id}
                        className={`mrow ${hasDupe ? 'has-dupe' : ''} ${isSel ? 'sel' : ''}`.replace(/\s+/g, ' ').trim()}
                        onClick={() => onSelect(m.id)}
                    >
                        <div className={`pdot ${m.pass === 'reaction' ? 'r' : ''}`.trim()} />
                        <span className="mname">{m.id}</span>
                        <span className={`rbadge ${m.router === 'entity' ? 'e' : ''}`.trim()}>
                            {m.router === 'broadcast' ? 'B' : 'E'}
                        </span>
                        {hasDupe && <span className="dupico" title="Potential duplicate">⚠</span>}
                        <span className="conscount">{m.consumers.length}</span>
                    </div>
                );
            })}
        </div>
    </div>
);
