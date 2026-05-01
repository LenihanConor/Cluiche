import { describe, it, expect } from 'vitest';
import { render } from '@testing-library/react';
import React from 'react';

vi.mock('reactflow', () => ({
    Handle: () => null,
    Position: { Top: 'top', Bottom: 'bottom' },
}));

import { PhaseNode } from './PhaseNode';

function makeProps(overrides: Partial<{
    phase: { instance_id: string; type: string; config?: object };
    modules: any[];
    isSelected: boolean;
    isCurrent: boolean;
}> = {}): any {
    return {
        id: 'node-id',
        data: {
            phase: { instance_id: 'TestPhase', type: 'UpdatePhase', config: {} },
            modules: [],
            puId: 'MainPU',
            isSelected: false,
            isCurrent: false,
            ...overrides,
        },
        xPos: 0, yPos: 0, selected: false, isConnectable: true, dragging: false, zIndex: 0,
    };
}

describe('PhaseNode – phaseColor', () => {
    const cases: [string, string][] = [
        ['InitPhase',     '#2e7d32'],
        ['BootPhase',     '#1b5e20'],
        ['UpdatePhase',   '#1565c0'],
        ['RunningPhase',  '#0d47a1'],
        ['RenderPhase',   '#e65100'],
        ['ShutdownPhase', '#b71c1c'],
        ['UnknownPhase',  '#37474f'],
    ];

    it.each(cases)('type %s → background %s', (type, expectedBg) => {
        const { container } = render(<PhaseNode {...makeProps({ phase: { instance_id: 'P', type } })} />);
        const div = container.firstElementChild as HTMLElement;
        // jsdom normalises hex to rgb() — compare via a temporary element
        const tmp = document.createElement('div');
        tmp.style.background = expectedBg;
        expect(div.style.background).toBe(tmp.style.background);
    });
});

describe('PhaseNode – border styles', () => {
    function normaliseColor(hex: string): string {
        const tmp = document.createElement('div');
        tmp.style.borderColor = hex;
        return tmp.style.borderColor;
    }

    it('default: 1px solid #555 border, no shadow', () => {
        const { container } = render(<PhaseNode {...makeProps()} />);
        const div = container.firstElementChild as HTMLElement;
        expect(div.style.borderColor).toBe(normaliseColor('#555'));
        expect(div.style.borderWidth).toBe('1px');
        expect(div.style.boxShadow).toBe('none');
    });

    it('isSelected: yellow border', () => {
        const { container } = render(<PhaseNode {...makeProps({ isSelected: true })} />);
        const div = container.firstElementChild as HTMLElement;
        expect(div.style.borderColor).toBe(normaliseColor('#ffd600'));
        expect(div.style.borderWidth).toBe('2px');
    });

    it('isCurrent: orange border with glow shadow', () => {
        const { container } = render(<PhaseNode {...makeProps({ isCurrent: true })} />);
        const div = container.firstElementChild as HTMLElement;
        expect(div.style.borderColor).toBe(normaliseColor('#ff6d00'));
        expect(div.style.boxShadow).toContain('255, 109, 0');
    });
});

describe('PhaseNode – module count', () => {
    it('hides module count when modules is empty', () => {
        const { container } = render(<PhaseNode {...makeProps({ modules: [] })} />);
        expect(container.textContent).not.toMatch(/module/);
    });

    it('shows singular "1 module"', () => {
        const mod = { instance_id: 'A', type: 'T', phases: [], config: {} };
        const { container } = render(<PhaseNode {...makeProps({ modules: [mod] })} />);
        expect(container.textContent).toContain('1 module');
    });

    it('shows plural "2 modules"', () => {
        const mods = [
            { instance_id: 'A', type: 'T', phases: [], config: {} },
            { instance_id: 'B', type: 'T', phases: [], config: {} },
        ];
        const { container } = render(<PhaseNode {...makeProps({ modules: mods })} />);
        expect(container.textContent).toContain('2 modules');
    });
});
