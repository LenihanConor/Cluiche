import { render, screen, fireEvent } from '@testing-library/react';
import ComponentPicker from './ComponentPicker';
import type { AvailableComponent } from '../types';

const mockPush = vi.fn();
vi.mock('@dia/editor-ui', () => ({
    theme: { bgPanel: '#252526', border: '#3c3c3c', text: '#d4d4d4', textMuted: '#888', accent: '#0e639c', error: '#f48771' },
    inputStyle: (o?: object) => ({ ...o }),
    buttonStyle: () => ({}),
    useToast: () => ({ push: mockPush }),
}));

const sampleComponents: AvailableComponent[] = [
    { typeId: 'TransformComponent', label: 'Transform', description: 'Position and rotation' },
    { typeId: 'SpriteComponent', label: 'Sprite', description: 'Visual sprite' },
    { typeId: 'PhysicsComponent', label: 'Physics', description: 'Rigid body physics' },
];

function makeProps(overrides?: Partial<Parameters<typeof ComponentPicker>[0]>) {
    return {
        isOpen: true,
        components: sampleComponents,
        usageCount: 0,
        sceneCount: 0,
        onAdd: vi.fn(),
        onClose: vi.fn(),
        ...overrides,
    };
}

beforeEach(() => {
    mockPush.mockClear();
});

describe('ComponentPicker', () => {
    it('returns null when isOpen=false', () => {
        const { container } = render(<ComponentPicker {...makeProps({ isOpen: false })} />);
        expect(container.firstChild).toBeNull();
    });

    it('renders picker-panel when isOpen=true', () => {
        render(<ComponentPicker {...makeProps()} />);
        expect(screen.getByTestId('picker-panel')).toBeInTheDocument();
    });

    it('search filter shows matching items only', () => {
        render(<ComponentPicker {...makeProps()} />);
        const input = screen.getByRole('textbox', { name: /search/i });
        fireEvent.change(input, { target: { value: 'sprite' } });
        expect(screen.getByTestId('picker-item-0')).toBeInTheDocument();
        expect(screen.queryByTestId('picker-item-1')).not.toBeInTheDocument();
        expect(screen.getByTestId('picker-count')).toHaveTextContent('1 of 3 components available');
    });

    it('search filter — no match shows empty message', () => {
        render(<ComponentPicker {...makeProps()} />);
        const input = screen.getByRole('textbox', { name: /search/i });
        fireEvent.change(input, { target: { value: 'xyz_no_match' } });
        expect(screen.getByText('No components match')).toBeInTheDocument();
        expect(screen.queryByTestId('picker-item-0')).not.toBeInTheDocument();
    });

    it('clicking item marks it as focused', () => {
        render(<ComponentPicker {...makeProps()} />);
        const item = screen.getByTestId('picker-item-1');
        fireEvent.click(item);
        // After clicking, add button should be enabled (focused item exists)
        expect(screen.getByTestId('add-btn')).not.toBeDisabled();
    });

    it('Add button disabled when no item focused', () => {
        render(<ComponentPicker {...makeProps()} />);
        expect(screen.getByTestId('add-btn')).toBeDisabled();
    });

    it('Add button enabled after item selected', () => {
        render(<ComponentPicker {...makeProps()} />);
        fireEvent.click(screen.getByTestId('picker-item-0'));
        expect(screen.getByTestId('add-btn')).not.toBeDisabled();
    });

    it('clicking Add calls onAdd with correct typeId', () => {
        const onAdd = vi.fn();
        render(<ComponentPicker {...makeProps({ onAdd })} />);
        fireEvent.click(screen.getByTestId('picker-item-1'));
        fireEvent.click(screen.getByTestId('add-btn'));
        expect(onAdd).toHaveBeenCalledOnce();
        expect(onAdd).toHaveBeenCalledWith('SpriteComponent');
    });

    it('clicking Cancel calls onClose', () => {
        const onClose = vi.fn();
        render(<ComponentPicker {...makeProps({ onClose })} />);
        fireEvent.click(screen.getByText('Cancel'));
        expect(onClose).toHaveBeenCalledOnce();
    });

    it('when usageCount > 0, Add shows toast warning', () => {
        const onAdd = vi.fn();
        render(<ComponentPicker {...makeProps({ onAdd, usageCount: 5, sceneCount: 2 })} />);
        fireEvent.click(screen.getByTestId('picker-item-0'));
        fireEvent.click(screen.getByTestId('add-btn'));
        expect(mockPush).toHaveBeenCalledOnce();
        expect(mockPush).toHaveBeenCalledWith(expect.stringContaining('5'), 'warning');
        expect(onAdd).toHaveBeenCalledOnce();
    });

    it('when usageCount = 0, no toast shown', () => {
        const onAdd = vi.fn();
        render(<ComponentPicker {...makeProps({ onAdd, usageCount: 0 })} />);
        fireEvent.click(screen.getByTestId('picker-item-0'));
        fireEvent.click(screen.getByTestId('add-btn'));
        expect(mockPush).not.toHaveBeenCalled();
        expect(onAdd).toHaveBeenCalledOnce();
    });

    it('double-clicking item calls onAdd immediately', () => {
        const onAdd = vi.fn();
        render(<ComponentPicker {...makeProps({ onAdd })} />);
        fireEvent.doubleClick(screen.getByTestId('picker-item-2'));
        expect(onAdd).toHaveBeenCalledOnce();
        expect(onAdd).toHaveBeenCalledWith('PhysicsComponent');
    });
});
