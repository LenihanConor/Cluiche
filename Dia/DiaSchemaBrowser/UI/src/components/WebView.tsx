import { useCallback, useEffect, useRef, useState } from 'react';
import type { FC } from 'react';
import type { Message } from '../model';
import {
    buildWebData, resetWebLayout, simTick, renderWeb, findNodeAt,
    type WebNode, type WebLink,
} from '../svg';

export interface WebViewProps {
    messages: Message[];
    active: boolean;
    onSelectMessage: (id: string) => void;
}

export const WebView: FC<WebViewProps> = ({ messages, active, onSelectMessage }) => {
    const svgRef = useRef<SVGSVGElement | null>(null);
    const nodesRef = useRef<WebNode[]>([]);
    const linksRef = useRef<WebLink[]>([]);
    const alphaRef = useRef(1.0);
    const rafRef = useRef<number | null>(null);
    const dragRef = useRef<WebNode | null>(null);
    const dragOffsetRef = useRef({ x: 0, y: 0 });
    const selectedRef = useRef<string | null>(null);
    const tracingRef = useRef(false);
    const builtRef = useRef(false);

    const [selInfo, setSelInfo] = useState('');
    const [selKind, setSelKind] = useState('');
    const [hint, setHint] = useState('Click any node to select · drag to reposition');
    const [tracing, setTracing] = useState(false);
    const [hasSelection, setHasSelection] = useState(false);

    const paint = useCallback(() => {
        const svg = svgRef.current;
        if (!svg) return;
        const W = svg.clientWidth || 900;
        svg.innerHTML = renderWeb(nodesRef.current, linksRef.current, W, selectedRef.current, tracingRef.current);
    }, []);

    const build = useCallback(() => {
        const svg = svgRef.current;
        if (!svg) return;
        const d = buildWebData(messages);
        nodesRef.current = d.nodes;
        linksRef.current = d.links;
        resetWebLayout(nodesRef.current, linksRef.current, svg.clientWidth || 900, svg.clientHeight || 460);
        alphaRef.current = 1.0;
        builtRef.current = true;
    }, [messages]);

    const startSim = useCallback(() => {
        const loop = () => {
            const svg = svgRef.current;
            if (svg) {
                alphaRef.current = simTick(
                    nodesRef.current, linksRef.current,
                    svg.clientWidth || 900, svg.clientHeight || 460,
                    alphaRef.current,
                );
            }
            paint();
            rafRef.current = requestAnimationFrame(loop);
        };
        if (rafRef.current == null) rafRef.current = requestAnimationFrame(loop);
    }, [paint]);

    const stopSim = useCallback(() => {
        if (rafRef.current != null) {
            cancelAnimationFrame(rafRef.current);
            rafRef.current = null;
        }
    }, []);

    // Rebuild when the message set changes.
    useEffect(() => {
        builtRef.current = false;
    }, [messages]);

    useEffect(() => {
        if (!active) {
            stopSim();
            return;
        }
        // Defer so the svg has been laid out.
        const t = setTimeout(() => {
            if (!builtRef.current) build();
            startSim();
        }, 50);
        return () => {
            clearTimeout(t);
            stopSim();
        };
    }, [active, build, startSim, stopSim]);

    const clearSelection = useCallback(() => {
        selectedRef.current = null;
        tracingRef.current = false;
        setTracing(false);
        setHasSelection(false);
        setSelInfo('');
        setSelKind('');
        setHint('Click any node to select · drag to reposition');
        alphaRef.current = Math.max(alphaRef.current, 0.05);
        paint();
    }, [paint]);

    const toggleTrace = useCallback(() => {
        if (!selectedRef.current) return;
        tracingRef.current = !tracingRef.current;
        setTracing(tracingRef.current);
        paint();
    }, [paint]);

    const resetLayout = useCallback(() => {
        const svg = svgRef.current;
        if (!svg) return;
        resetWebLayout(nodesRef.current, linksRef.current, svg.clientWidth || 900, svg.clientHeight || 460);
        alphaRef.current = 1.0;
        startSim();
    }, [startSim]);

    const localXY = (e: React.MouseEvent) => {
        const rect = (e.currentTarget as SVGSVGElement).getBoundingClientRect();
        return { x: e.clientX - rect.left, y: e.clientY - rect.top };
    };

    const onMouseDown = (e: React.MouseEvent) => {
        const { x, y } = localXY(e);
        const n = findNodeAt(nodesRef.current, x, y);
        if (n) {
            dragRef.current = n;
            dragOffsetRef.current = { x: x - n.x, y: y - n.y };
            n.fixed = true;
            alphaRef.current = Math.max(alphaRef.current, 0.2);
            e.preventDefault();
        }
    };

    const onMouseMove = (e: React.MouseEvent) => {
        const n = dragRef.current;
        if (!n) return;
        const { x, y } = localXY(e);
        n.x = x - dragOffsetRef.current.x;
        n.y = y - dragOffsetRef.current.y;
    };

    const onMouseUp = () => {
        dragRef.current = null;
    };

    const onClick = (e: React.MouseEvent) => {
        // If a drag just happened, findNodeAt still resolves — mirror mockup:
        // treat as select unless we were actively dragging (cleared on mouseup).
        const { x, y } = localXY(e);
        const n = findNodeAt(nodesRef.current, x, y);
        if (!n) {
            clearSelection();
            return;
        }
        if (n.id === selectedRef.current) {
            clearSelection();
            return;
        }
        selectedRef.current = n.id;
        setHasSelection(true);
        setHint('');
        if (n.kind === 'message') {
            const msg = messages.find((m) => m.id === n.msgId);
            setSelInfo(n.label);
            setSelKind(msg ? `${msg.pass} · ${msg.router}` : '');
            if (msg) onSelectMessage(msg.id);
        } else {
            setSelInfo(n.label);
            setSelKind(n.kind);
        }
        paint();
    };

    return (
        <div className={`web-canvas ${active ? 'on' : ''}`.trim()}>
            <div className="web-toolbar">
                <button
                    className={`wtbtn ${tracing ? 'active' : ''}`.trim()}
                    onClick={toggleTrace}
                    disabled={!hasSelection}
                >
                    Trace Chain
                </button>
                <button className="wtbtn" onClick={clearSelection}>Clear</button>
                <button className="wtbtn" onClick={resetLayout}>Reset Layout</button>
                <span className="web-hint">{hint}</span>
                <span className="web-selected-info">
                    {selInfo && (<><span>{selInfo}</span>{selKind ? ` · ${selKind}` : ''}</>)}
                </span>
                <div className="chain-legend">
                    <div className="cl-item"><div className="cl-dot" style={{ background: 'var(--producer)' }} />System</div>
                    <div className="cl-item"><div className="cl-dot" style={{ background: 'var(--entity)' }} />Component</div>
                    <div className="cl-item"><div className="cl-dot" style={{ background: 'var(--adapter)' }} />Adapter</div>
                    <div className="cl-item"><div className="cl-dot" style={{ transform: 'rotate(45deg)', borderRadius: '1px', background: 'var(--primary)' }} />Primary msg</div>
                    <div className="cl-item"><div className="cl-dot" style={{ transform: 'rotate(45deg)', borderRadius: '1px', background: 'var(--reaction)' }} />Reaction msg</div>
                </div>
            </div>
            <svg
                id="web-svg"
                ref={svgRef}
                onMouseDown={onMouseDown}
                onMouseMove={onMouseMove}
                onMouseUp={onMouseUp}
                onClick={onClick}
            />
        </div>
    );
};
