// svg.ts — SVG string builders + force-directed layout, ported verbatim (in
// behaviour) from the approved mockup's renderGraph / buildWebData / simTick /
// renderWeb, parameterised over the real Message model instead of static data.

import type { Message } from './model';
import { producerIsAdapter, consumerIsComponent } from './model';

// ── GRAPH VIEW ────────────────────────────────────────────────────────────────

export function buildGraphSvg(msg: Message | undefined, W: number, H: number): string {
    if (!msg) return '';
    const cx = W / 2;
    const cy = H / 2;
    const passColor = msg.pass === 'primary' ? '#F0AC1A' : '#9B72F5';
    const routerColor = msg.router === 'broadcast' ? '#1ED8D8' : '#4A7FE0';
    const dHW = Math.max(80, msg.id.length * 5 + 36);
    const dHH = 38;
    const nodeW = 148;
    const nodeH = 28;
    const spacing = 46;
    const prodX = W * 0.18;
    const consX = W * 0.82;
    const prodN = msg.producers.length;
    const consN = msg.consumers.length;
    const prodY0 = cy - ((prodN - 1) * spacing) / 2;
    const consY0 = cy - ((consN - 1) * spacing) / 2;

    let s = `<defs>
    <marker id="arr" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto"><polygon points="0 0,7 3.5,0 7" fill="${passColor}" opacity=".65"/></marker>
    <filter id="glow"><feGaussianBlur stdDeviation="3" result="cb"/><feMerge><feMergeNode in="cb"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
  </defs>`;

    msg.producers.forEach((_, i) => {
        const py = prodY0 + i * spacing;
        const x1 = prodX + nodeW / 2;
        const y1 = py;
        const x2 = cx - dHW;
        const y2 = cy;
        const dx = Math.abs(x2 - x1) * 0.42;
        s += `<path d="M${x1} ${y1} C${x1 + dx} ${y1},${x2 - dx} ${y2},${x2} ${y2}" fill="none" stroke="${passColor}" stroke-width="1.5" opacity=".45" marker-end="url(#arr)"/>`;
    });
    msg.consumers.forEach((_, i) => {
        const cy2 = consY0 + i * spacing;
        const x1 = cx + dHW;
        const y1 = cy;
        const x2 = consX - nodeW / 2;
        const y2 = cy2;
        const dx = Math.abs(x2 - x1) * 0.42;
        s += `<path d="M${x1} ${y1} C${x1 + dx} ${y1},${x2 - dx} ${y2},${x2} ${y2}" fill="none" stroke="${passColor}" stroke-width="1.5" opacity=".45" marker-end="url(#arr)"/>`;
    });
    msg.producers.forEach((prod, i) => {
        const py = prodY0 + i * spacing;
        const isA = producerIsAdapter(prod);
        const nc = isA ? '#F0AC1A' : '#2ECC8A';
        const lbl = isA ? 'ADAPTER' : 'PRODUCER';
        const x = prodX - nodeW / 2;
        const y = py - nodeH / 2;
        s += `<rect x="${x}" y="${y}" width="${nodeW}" height="${nodeH}" fill="#1E2340" stroke="${nc}" stroke-width="1.5"/>
    <text x="${x + 5}" y="${y + 8}" font-family="JetBrains Mono,monospace" font-size="7" fill="${nc}" opacity=".55" letter-spacing=".1">${lbl}</text>
    <text x="${prodX}" y="${py + 4}" text-anchor="middle" font-family="JetBrains Mono,monospace" font-size="11" font-weight="500" fill="${nc}">${prod}</text>`;
    });

    const dp = `M${cx} ${cy - dHH} L${cx + dHW} ${cy} L${cx} ${cy + dHH} L${cx - dHW} ${cy} Z`;
    s += `<path d="${dp}" fill="#171B2F" stroke="${passColor}" stroke-width="2" filter="url(#glow)" opacity=".9"/>
  <path d="${dp}" fill="#171B2F" stroke="${passColor}" stroke-width="2"/>
  <text x="${cx}" y="${cy - 5}" text-anchor="middle" font-family="JetBrains Mono,monospace" font-size="12" font-weight="600" fill="${routerColor}">${msg.id}</text>
  <text x="${cx}" y="${cy + 13}" text-anchor="middle" font-family="JetBrains Mono,monospace" font-size="8" fill="${passColor}" opacity=".7" letter-spacing=".1">${msg.pass.toUpperCase()} · ${msg.router.toUpperCase()}</text>`;

    msg.consumers.forEach((cons, i) => {
        const cy2 = consY0 + i * spacing;
        const isComp = consumerIsComponent(msg.router, cons);
        const nc = isComp ? '#4A7FE0' : '#1ED8D8';
        const lbl = isComp ? 'COMPONENT' : 'SUBSCRIBER';
        const x = consX - nodeW / 2;
        const y = cy2 - nodeH / 2;
        s += `<rect x="${x}" y="${y}" width="${nodeW}" height="${nodeH}" fill="#1E2340" stroke="${nc}" stroke-width="1"/>
    <text x="${x + 5}" y="${y + 8}" font-family="JetBrains Mono,monospace" font-size="7" fill="${nc}" opacity=".55" letter-spacing=".1">${lbl}</text>
    <text x="${consX}" y="${cy2 + 4}" text-anchor="middle" font-family="JetBrains Mono,monospace" font-size="11" fill="${nc}">${cons}</text>`;
    });

    return s;
}

// ── WEB VIEW ──────────────────────────────────────────────────────────────────

export interface WebNode {
    id: string;
    label: string;
    kind: 'system' | 'component' | 'adapter' | 'message';
    pass?: string;
    router?: string;
    msgId?: string;
    x: number;
    y: number;
    vx: number;
    vy: number;
    fixed: boolean;
}

export interface WebLink {
    source: string;
    target: string;
    type: 'produce' | 'consume';
    pass: string;
    msgId: string;
}

export interface WebData {
    nodes: WebNode[];
    links: WebLink[];
}

export function buildWebData(messages: Message[]): WebData {
    const nodeMap: Record<string, WebNode> = {};
    const links: WebLink[] = [];
    messages.forEach((m) => {
        if (!nodeMap['msg:' + m.id]) {
            nodeMap['msg:' + m.id] = {
                id: 'msg:' + m.id,
                label: m.id,
                kind: 'message',
                pass: m.pass,
                router: m.router,
                msgId: m.id,
                x: 0, y: 0, vx: 0, vy: 0, fixed: false,
            };
        }
        [...m.producers, ...m.consumers].forEach((name) => {
            if (!nodeMap['sys:' + name]) {
                const isComp = name.endsWith('Comp');
                const isAdap = name.includes('Adapter');
                nodeMap['sys:' + name] = {
                    id: 'sys:' + name,
                    label: name,
                    kind: isAdap ? 'adapter' : isComp ? 'component' : 'system',
                    x: 0, y: 0, vx: 0, vy: 0, fixed: false,
                };
            }
        });
        m.producers.forEach((p) => links.push({ source: 'sys:' + p, target: 'msg:' + m.id, type: 'produce', pass: m.pass, msgId: m.id }));
        m.consumers.forEach((c) => links.push({ source: 'msg:' + m.id, target: 'sys:' + c, type: 'consume', pass: m.pass, msgId: m.id }));
    });
    return { nodes: Object.values(nodeMap), links };
}

export function resetWebLayout(nodes: WebNode[], links: WebLink[], W: number, H: number): void {
    const cx = W / 2;
    const cy = H / 2;
    const sysNodes = nodes.filter((n) => n.kind !== 'message');
    const msgNodes = nodes.filter((n) => n.kind === 'message');
    const r = Math.min(W, H) * 0.36;
    sysNodes.forEach((n, i) => {
        const a = (i / sysNodes.length) * 2 * Math.PI - Math.PI / 2;
        n.x = cx + r * Math.cos(a);
        n.y = cy + r * Math.sin(a);
        n.vx = 0; n.vy = 0; n.fixed = false;
    });
    msgNodes.forEach((n) => {
        const conns = links.filter((l) => l.source === n.id || l.target === n.id);
        if (!conns.length) {
            n.x = cx; n.y = cy;
        } else {
            const pts = conns.map((l) => {
                const oid = l.source === n.id ? l.target : l.source;
                const on = nodes.find((nn) => nn.id === oid);
                return on ? { x: on.x, y: on.y } : { x: cx, y: cy };
            });
            n.x = pts.reduce((s, p) => s + p.x, 0) / pts.length;
            n.y = pts.reduce((s, p) => s + p.y, 0) / pts.length;
        }
        n.vx = 0; n.vy = 0; n.fixed = false;
    });
}

export function simTick(nodes: WebNode[], links: WebLink[], W: number, H: number, simAlpha: number): number {
    if (simAlpha < 0.005) return simAlpha;
    const repulsion = 1800;
    const springK = 0.04;
    const idealLen = 110;
    const centerK = 0.012;
    const damp = 0.82;
    const cx = W / 2;
    const cy = H / 2;
    for (let i = 0; i < nodes.length; i++) {
        for (let j = i + 1; j < nodes.length; j++) {
            const a = nodes[i];
            const b = nodes[j];
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const d = Math.sqrt(dx * dx + dy * dy) || 1;
            const f = repulsion / (d * d);
            const fx = (dx / d) * f;
            const fy = (dy / d) * f;
            if (!a.fixed) { a.vx -= fx; a.vy -= fy; }
            if (!b.fixed) { b.vx += fx; b.vy += fy; }
        }
    }
    links.forEach((l) => {
        const a = nodes.find((n) => n.id === l.source);
        const b = nodes.find((n) => n.id === l.target);
        if (!a || !b) return;
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const d = Math.sqrt(dx * dx + dy * dy) || 1;
        const f = springK * (d - idealLen);
        const fx = (dx / d) * f;
        const fy = (dy / d) * f;
        if (!a.fixed) { a.vx += fx; a.vy += fy; }
        if (!b.fixed) { b.vx -= fx; b.vy -= fy; }
    });
    nodes.forEach((n) => {
        if (n.fixed) return;
        n.vx += (cx - n.x) * centerK;
        n.vy += (cy - n.y) * centerK;
        n.vx *= damp * simAlpha;
        n.vy *= damp * simAlpha;
        n.x += n.vx;
        n.y += n.vy;
        n.x = Math.max(30, Math.min(W - 30, n.x));
        n.y = Math.max(30, Math.min(H - 30, n.y));
    });
    return simAlpha * 0.985;
}

export function getNodeColor(n: WebNode): string {
    if (n.kind === 'message') return n.pass === 'primary' ? '#F0AC1A' : '#9B72F5';
    if (n.kind === 'adapter') return '#F0AC1A';
    if (n.kind === 'component') return '#4A7FE0';
    return '#1ED8D8';
}

export function getNodeR(n: WebNode): number {
    return n.kind === 'message' ? 13 : n.kind === 'system' ? 20 : 16;
}

export interface Highlight {
    nodes: Set<string>;
    links: Set<number>;
}

export function computeHighlight(links: WebLink[], selected: string | null): Highlight | null {
    if (!selected) return null;
    const hnodes = new Set<string>([selected]);
    const hlinks = new Set<number>();
    links.forEach((l, i) => {
        if (l.source === selected || l.target === selected) {
            hlinks.add(i);
            hnodes.add(l.source);
            hnodes.add(l.target);
        }
    });
    return { nodes: hnodes, links: hlinks };
}

export function computeChain(links: WebLink[], startId: string): Highlight {
    const hnodes = new Set<string>([startId]);
    const hlinks = new Set<number>();
    const queue: string[] = [startId];
    const visited = new Set<string>();
    while (queue.length) {
        const cur = queue.shift()!;
        if (visited.has(cur)) continue;
        visited.add(cur);
        links.forEach((l, i) => {
            if (l.source === cur) {
                hlinks.add(i);
                hnodes.add(l.target);
                if (!visited.has(l.target)) queue.push(l.target);
            }
        });
    }
    return { nodes: hnodes, links: hlinks };
}

export function findNodeAt(nodes: WebNode[], x: number, y: number): WebNode | null {
    for (let i = nodes.length - 1; i >= 0; i--) {
        const n = nodes[i];
        const r = getNodeR(n) + 5;
        if (Math.abs(x - n.x) < r && Math.abs(y - n.y) < r) return n;
    }
    return null;
}

export function renderWeb(
    nodes: WebNode[],
    links: WebLink[],
    _W: number,
    selected: string | null,
    tracing: boolean,
): string {
    const hl = tracing && selected ? computeChain(links, selected) : computeHighlight(links, selected);
    const hasHl = hl !== null;
    const MF = 'JetBrains Mono,monospace';

    let s = `<defs>
    <marker id="wa-p" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><polygon points="0 0,6 3,0 6" fill="#F0AC1A" opacity=".7"/></marker>
    <marker id="wa-r" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><polygon points="0 0,6 3,0 6" fill="#9B72F5" opacity=".7"/></marker>
    <marker id="wa-pd" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><polygon points="0 0,6 3,0 6" fill="#F0AC1A" opacity=".25"/></marker>
    <marker id="wa-rd" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><polygon points="0 0,6 3,0 6" fill="#9B72F5" opacity=".25"/></marker>
    <filter id="wglow"><feGaussianBlur stdDeviation="4" result="cb"/><feMerge><feMergeNode in="cb"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
  </defs>`;

    links.forEach((l, i) => {
        const a = nodes.find((n) => n.id === l.source);
        const b = nodes.find((n) => n.id === l.target);
        if (!a || !b) return;
        const lit = !hasHl || hl!.links.has(i);
        const col = l.pass === 'primary' ? '#F0AC1A' : '#9B72F5';
        const op = lit ? 0.55 : 0.06;
        const markId = lit ? (l.pass === 'primary' ? 'wa-p' : 'wa-r') : (l.pass === 'primary' ? 'wa-pd' : 'wa-rd');
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const len = Math.sqrt(dx * dx + dy * dy) || 1;
        const br = getNodeR(b) + 3;
        const tx = b.x - (dx / len) * br;
        const ty = b.y - (dy / len) * br;
        const mx = (a.x + b.x) / 2 + (dy / len) * 18;
        const my = (a.y + b.y) / 2 - (dx / len) * 18;
        s += `<path d="M${a.x} ${a.y} Q${mx} ${my} ${tx} ${ty}" fill="none" stroke="${col}" stroke-width="${lit ? 1.5 : 1}" opacity="${op}" marker-end="url(#${markId})"/>`;
    });

    nodes.forEach((n) => {
        const lit = !hasHl || hl!.nodes.has(n.id);
        const isSel = n.id === selected;
        const col = getNodeColor(n);
        const r = getNodeR(n);
        const baseOp = lit ? 1 : 0.18;
        const strokeW = isSel ? 2.5 : 1.5;

        if (n.kind === 'message') {
            const s2 = r;
            const pts = `${n.x} ${n.y - s2} ${n.x + s2} ${n.y} ${n.x} ${n.y + s2} ${n.x - s2} ${n.y}`;
            const fill = isSel ? '#222844' : '#111420';
            s += `<polygon points="${pts}" fill="${fill}" stroke="${col}" stroke-width="${strokeW}" opacity="${baseOp}" ${isSel ? 'filter="url(#wglow)"' : ''}/>`;
            s += `<text x="${n.x}" y="${n.y + 3}" text-anchor="middle" font-family="${MF}" font-size="8" font-weight="500" fill="${col}" opacity="${baseOp}">${n.label}</text>`;
        } else {
            s += `<circle cx="${n.x}" cy="${n.y}" r="${r}" fill="${isSel ? '#1E2340' : '#111420'}" stroke="${col}" stroke-width="${strokeW}" opacity="${baseOp}" ${isSel ? 'filter="url(#wglow)"' : ''}/>`;
            const short = n.label.replace(/System|Comp|Adapter/, '');
            s += `<text x="${n.x}" y="${n.y + 4}" text-anchor="middle" font-family="${MF}" font-size="${n.kind === 'system' ? 9 : 8}" font-weight="500" fill="${col}" opacity="${baseOp}">${short}</text>`;
        }
    });

    return s;
}
