import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { PropertyPanel } from './PropertyPanel';
import type { BlueprintProperties } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: { bg: '#1e1e1e', border: '#3c3c3c', text: '#d4d4d4', textMuted: '#888' },
    buttonStyle: () => ({}),
    EmptyState: ({ message }: { message: string }) => <div data-testid="empty-state">{message}</div>,
}));

vi.mock('./ComponentAccordion', () => ({
    default: ({ component, onRemove, children }: any) => (
        <div data-testid={`accordion-${component.type}`}>
            <button data-testid={`remove-${component.type}`} onClick={() => onRemove(component.type)} />
            {children}
        </div>
    ),
}));

vi.mock('./FieldRow', () => ({
    FieldRow: ({ field }: any) => <div data-testid={`field-${field.name}`} />,
}));

const sampleProperties: BlueprintProperties = {
    id: 'hero',
    components: [
        {
            type: 'Transform',
            fields: [
                { name: 'x', kind: 'float', value: 0 },
                { name: 'y', kind: 'float', value: 0 },
            ],
        },
        {
            type: 'Sprite',
            fields: [
                { name: 'texture', kind: 'string', value: 'hero.png' },
            ],
        },
    ],
};

function makeProps(overrides?: Partial<Parameters<typeof PropertyPanel>[0]>) {
    return {
        properties: sampleProperties,
        selectedPath: null,
        onFieldChange: vi.fn(),
        onRemoveComponent: vi.fn(),
        onAddComponentClick: vi.fn(),
        ...overrides,
    };
}

describe('PropertyPanel', () => {
    it('shows EmptyState when properties is null', () => {
        render(<PropertyPanel {...makeProps({ properties: null })} />);
        expect(screen.getByTestId('empty-state')).toBeInTheDocument();
        expect(screen.getByTestId('empty-state')).toHaveTextContent('Select a blueprint');
    });

    it('shows blueprint-id when properties provided', () => {
        render(<PropertyPanel {...makeProps()} />);
        expect(screen.getByTestId('blueprint-id')).toBeInTheDocument();
        expect(screen.getByTestId('blueprint-id')).toHaveTextContent('⬡ hero');
    });

    it('renders one accordion per component', () => {
        render(<PropertyPanel {...makeProps()} />);
        expect(screen.getByTestId('accordion-Transform')).toBeInTheDocument();
        expect(screen.getByTestId('accordion-Sprite')).toBeInTheDocument();
    });

    it('renders FieldRows inside each accordion', () => {
        render(<PropertyPanel {...makeProps()} />);
        expect(screen.getByTestId('field-x')).toBeInTheDocument();
        expect(screen.getByTestId('field-y')).toBeInTheDocument();
        expect(screen.getByTestId('field-texture')).toBeInTheDocument();
    });

    it('onRemoveComponent called with correct type', async () => {
        const onRemoveComponent = vi.fn();
        render(<PropertyPanel {...makeProps({ onRemoveComponent })} />);

        await userEvent.click(screen.getByTestId('remove-Transform'));

        expect(onRemoveComponent).toHaveBeenCalledWith('Transform');
    });

    it('onAddComponentClick called when add button clicked', async () => {
        const onAddComponentClick = vi.fn();
        render(<PropertyPanel {...makeProps({ onAddComponentClick })} />);

        await userEvent.click(screen.getByTestId('add-component-btn'));

        expect(onAddComponentClick).toHaveBeenCalledTimes(1);
    });

    it('no accordion rendered when components array is empty but id header still shown', () => {
        const emptyProperties: BlueprintProperties = { id: 'empty-bp', components: [] };
        render(<PropertyPanel {...makeProps({ properties: emptyProperties })} />);

        expect(screen.getByTestId('blueprint-id')).toHaveTextContent('⬡ empty-bp');
        expect(screen.queryByTestId(/^accordion-/)).not.toBeInTheDocument();
    });
});
