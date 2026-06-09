import { render, screen, fireEvent } from '@testing-library/react';
import { vi, beforeEach, describe, it, expect } from 'vitest';
import { FieldsTab } from './FieldsTab';
import type { EntityEntry } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: {
        bg: '#1e1e1e', bgPanel: '#252526', bgInput: '#2d2d2d', border: '#3c3c3c',
        borderMuted: '#555', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c',
        accentHover: '#007acc', success: '#89d185', warning: '#cca700', error: '#f48771',
        textDim: '#ccc',
    },
    injectThemeVars: vi.fn(),
    inputStyle: () => ({}),
    buttonStyle: () => ({}),
}));

const PARENT: EntityEntry = { i: 5, g: 1, n: 'Root', t: ['T'], d: 0 };

const ENTITY: EntityEntry = {
    i: 0, g: 1, n: 'Player', t: ['T', 'P'], d: 1,
    pi: 5,
    components: [
        {
            name: 'Transform',
            tier: 'A',
            fields: [
                { n: 'x', v: '10.5', t: 'float', dt: 'float' },
                { n: 'y', v: '20.0', t: 'float', dt: 'float' },
            ],
        },
        {
            name: 'Physics',
            fields: [
                { n: 'mass', v: '1.0', t: 'num', dt: 'f32' },
            ],
        },
        {
            name: 'ThirdComp',
            fields: [
                { n: 'active', v: 'true', t: 'bool', dt: 'bool' },
            ],
        },
    ],
};

const ALL_ENTITIES = [PARENT, ENTITY];

function mkProps(overrides: Partial<Parameters<typeof FieldsTab>[0]> = {}) {
    return {
        entity: ENTITY,
        allEntities: ALL_ENTITIES,
        onNavigate: vi.fn(),
        onWriteField: vi.fn(),
        ...overrides,
    };
}

beforeEach(() => {
    vi.clearAllMocks();
});

describe('FieldsTab', () => {
    it('shows "Select an entity" when entity is null', () => {
        render(<FieldsTab {...mkProps({ entity: null })} />);
        expect(screen.getByText(/select an entity/i)).toBeInTheDocument();
    });

    it('renders component accordion with component names', () => {
        render(<FieldsTab {...mkProps()} />);
        expect(screen.getByTestId('component-header-Transform')).toBeInTheDocument();
        expect(screen.getByTestId('component-header-Physics')).toBeInTheDocument();
        expect(screen.getByTestId('component-header-ThirdComp')).toBeInTheDocument();
    });

    it('first 2 components start expanded, third starts collapsed', () => {
        render(<FieldsTab {...mkProps()} />);
        // Transform (index 0) and Physics (index 1) open by default — their fields visible
        expect(screen.getByTestId('field-row-Transform-x')).toBeInTheDocument();
        expect(screen.getByTestId('field-row-Physics-mass')).toBeInTheDocument();
        // ThirdComp (index 2) closed by default
        expect(screen.queryByTestId('field-row-ThirdComp-active')).toBeNull();
    });

    it('clicking component header toggles expanded state', () => {
        render(<FieldsTab {...mkProps()} />);
        // Transform is open — clicking closes it
        fireEvent.click(screen.getByTestId('component-header-Transform'));
        expect(screen.queryByTestId('field-row-Transform-x')).toBeNull();
        // click again to reopen
        fireEvent.click(screen.getByTestId('component-header-Transform'));
        expect(screen.getByTestId('field-row-Transform-x')).toBeInTheDocument();
    });

    it('clicking edit pencil shows inline input', () => {
        render(<FieldsTab {...mkProps()} />);
        fireEvent.click(screen.getByTestId('field-edit-btn-Transform-x'));
        expect(screen.getByTestId('field-edit-input-Transform-x')).toBeInTheDocument();
    });

    it('pressing Enter on inline input calls onWriteField', () => {
        const onWriteField = vi.fn();
        render(<FieldsTab {...mkProps({ onWriteField })} />);
        fireEvent.click(screen.getByTestId('field-edit-btn-Transform-x'));
        const input = screen.getByTestId('field-edit-input-Transform-x');
        fireEvent.change(input, { target: { value: '99.9' } });
        fireEvent.keyDown(input, { key: 'Enter' });
        expect(onWriteField).toHaveBeenCalledWith(0, 'Transform', 'x', '99.9');
    });

    it('pressing Escape cancels edit (reverts to display)', () => {
        const onWriteField = vi.fn();
        render(<FieldsTab {...mkProps({ onWriteField })} />);
        fireEvent.click(screen.getByTestId('field-edit-btn-Transform-x'));
        const input = screen.getByTestId('field-edit-input-Transform-x');
        fireEvent.change(input, { target: { value: 'bad' } });
        fireEvent.keyDown(input, { key: 'Escape' });
        expect(onWriteField).not.toHaveBeenCalled();
        // Input gone, display value still shows original
        expect(screen.queryByTestId('field-edit-input-Transform-x')).toBeNull();
        expect(screen.getByText('10.5')).toBeInTheDocument();
    });

    it('hierarchy nav shows parent link when entity has parent (pi >= 0)', () => {
        render(<FieldsTab {...mkProps()} />);
        expect(screen.getByTestId('parent-link')).toBeInTheDocument();
        expect(screen.getByTestId('parent-link').textContent).toBe('Root');
    });

    it('clicking parent link calls onNavigate', () => {
        const onNavigate = vi.fn();
        render(<FieldsTab {...mkProps({ onNavigate })} />);
        fireEvent.click(screen.getByTestId('parent-link'));
        expect(onNavigate).toHaveBeenCalledWith(5);
    });

    it('hierarchy nav is hidden when entity has no parent and no children', () => {
        const noHierEntity: EntityEntry = {
            i: 10, g: 1, n: 'Loner', t: [], d: 0,
            components: [],
        };
        render(<FieldsTab entity={noHierEntity} allEntities={[noHierEntity]} onNavigate={vi.fn()} onWriteField={vi.fn()} />);
        expect(screen.queryByTestId('hierarchy-nav')).toBeNull();
    });
});
