import type { FC } from 'react';
import type { Message, SchemaModel, DiffRow } from '../model';
import { computeDiff } from '../model';

export interface AnalysisTabProps {
    message: Message | undefined;
    model: SchemaModel;
    active: boolean;
}

function diffCellClass(row: DiffRow): { left: string; sym: string; symbol: string } {
    switch (row.kind) {
        case 'match': return { left: 'd-match', sym: 'd-sym', symbol: '=' };
        case 'differ': return { left: 'd-left', sym: 'd-sym', symbol: '≠' };
        case 'left': return { left: 'd-left', sym: 'd-sym', symbol: '−' };
        case 'right': return { left: 'd-eq', sym: 'd-sym', symbol: '+' };
    }
}

export const AnalysisTab: FC<AnalysisTabProps> = ({ message, model, active }) => {
    const { orphans } = model;
    const orphanTotal =
        orphans.producedNeverConsumed.length +
        orphans.consumedNeverProduced.length +
        orphans.isolated.length;

    const dupeCards = message
        ? message.dupes
              .map((otherId) => model.byId[otherId])
              .filter((o): o is Message => Boolean(o))
              .map((other) => ({ other, diff: computeDiff(message, other) }))
        : [];

    const nothing = dupeCards.length === 0 && orphanTotal === 0;

    return (
        <div className={`analysis-view ${active ? 'on' : ''}`.trim()}>
            {nothing && <div className="an-empty">No duplicate candidates or orphans detected.</div>}

            {message && dupeCards.length > 0 && (
                <>
                    <div className="an-section-label">Structural Duplicate Candidates — {message.id}</div>
                    {dupeCards.map(({ other, diff }) => (
                        <div className="dupe-card" key={other.id}>
                            <div className="dupe-head">
                                <span className="dupe-ttl">{message.id} ≈ {other.id}</span>
                                <span className="dupe-score">{Math.round(diff.score * 100)}% field match</span>
                                {diff.sameSubscribers && (
                                    <span
                                        className="dupe-score"
                                        style={{
                                            color: 'var(--primary)',
                                            borderColor: 'rgba(240,172,26,.3)',
                                            background: 'rgba(240,172,26,.1)',
                                        }}
                                    >
                                        same subscribers
                                    </span>
                                )}
                            </div>
                            <table className="diff-table">
                                <thead>
                                    <tr>
                                        <th>{message.id}</th>
                                        <th>type</th>
                                        <th />
                                        <th>{other.id}</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {diff.rows.map((row) => {
                                        const c = diffCellClass(row);
                                        if (row.kind === 'match' || row.kind === 'differ') {
                                            return (
                                                <tr key={row.name}>
                                                    <td className={c.left}>{row.name}</td>
                                                    <td className={c.left}>{row.aType}</td>
                                                    <td className={c.sym}>{c.symbol}</td>
                                                    <td className={row.kind === 'match' ? 'd-match' : 'd-right'}>{row.bType}</td>
                                                </tr>
                                            );
                                        }
                                        if (row.kind === 'left') {
                                            return (
                                                <tr key={row.name}>
                                                    <td className="d-left">{row.name}</td>
                                                    <td className="d-left">{row.aType}</td>
                                                    <td className="d-sym">−</td>
                                                    <td className="d-eq">—</td>
                                                </tr>
                                            );
                                        }
                                        return (
                                            <tr key={row.name}>
                                                <td className="d-eq">—</td>
                                                <td className="d-eq">—</td>
                                                <td className="d-sym">+</td>
                                                <td className="d-right">{row.name}: {row.bType}</td>
                                            </tr>
                                        );
                                    })}
                                </tbody>
                            </table>
                            <div className="dupe-suggest">
                                ↳ Consider merging into <code>{message.id}</code> with a discriminator tag field (e.g. <code>kind: StringCRC</code>)
                            </div>
                        </div>
                    ))}
                </>
            )}

            {orphanTotal > 0 && (
                <>
                    <div className="an-section-label">Orphan Analysis (all message types)</div>
                    {orphans.producedNeverConsumed.map((id) => (
                        <div className="orphan-row" key={`nc-${id}`}>
                            <span className="o-id">{id}</span>
                            <span className="orphan-tag noc">produced · no consumer</span>
                        </div>
                    ))}
                    {orphans.consumedNeverProduced.map((id) => (
                        <div className="orphan-row" key={`np-${id}`}>
                            <span className="o-id">{id}</span>
                            <span className="orphan-tag nop">consumed · no producer</span>
                        </div>
                    ))}
                    {orphans.isolated.map((id) => (
                        <div className="orphan-row" key={`iso-${id}`}>
                            <span className="o-id">{id}</span>
                            <span className="orphan-tag nop">no producer · no consumer</span>
                        </div>
                    ))}
                </>
            )}
        </div>
    );
};
