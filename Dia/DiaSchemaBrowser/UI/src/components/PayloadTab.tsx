import type { FC } from 'react';
import type { Message } from '../model';

export interface PayloadTabProps {
    message: Message | undefined;
    active: boolean;
}

export const PayloadTab: FC<PayloadTabProps> = ({ message, active }) => {
    if (!message) {
        return (
            <div className={`payload-view ${active ? 'on' : ''}`.trim()}>
                <div className="py-empty">Select a message type to inspect its payload.</div>
            </div>
        );
    }
    return (
        <div className={`payload-view ${active ? 'on' : ''}`.trim()}>
            {message.payload.length === 0 ? (
                <div className="py-empty">This message declares no payload fields.</div>
            ) : (
                <table className="field-table">
                    <thead>
                        <tr>
                            <th>Field</th>
                            <th>Type</th>
                            <th>Notes</th>
                        </tr>
                    </thead>
                    <tbody>
                        {message.payload.map((f) => (
                            <tr key={f.name}>
                                <td className="fname">{f.name}</td>
                                <td className="ftype">{f.type}</td>
                                <td className="fnotes">{f.notes}</td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            )}
        </div>
    );
};
