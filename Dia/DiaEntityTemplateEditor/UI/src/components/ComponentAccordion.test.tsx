import { render, screen, fireEvent } from '@testing-library/react';
import ComponentAccordion from './ComponentAccordion';
import type { ComponentEntry } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: { bgPanel: '#252526', border: '#3c3c3c', text: '#d4d4d4', textMuted: '#888', error: '#f48771' },
    buttonStyle: (_v?: string, o?: object) => ({ ...o }),
}));

const mockComponent: ComponentEntry = {
    type: 'TransformComponent',
    fields: [
        { name: 'position', kind: 'Vec3' },
        { name: 'rotation', kind: 'float' },
    ],
};

describe('ComponentAccordion', () => {
    it('renders component type name in header', () => {
        render(
            <ComponentAccordion component={mockComponent} onRemove={vi.fn()} />
        );
        expect(screen.getByTestId('accordion-header-TransformComponent')).toHaveTextContent('TransformComponent');
    });

    it('body is visible by default (open=true)', () => {
        render(
            <ComponentAccordion component={mockComponent} onRemove={vi.fn()} />
        );
        const body = screen.getByTestId('accordion-body-TransformComponent');
        expect(body).toBeVisible();
        expect(body).not.toHaveStyle({ display: 'none' });
    });

    it('clicking header toggles body hidden', () => {
        render(
            <ComponentAccordion component={mockComponent} onRemove={vi.fn()} />
        );
        const header = screen.getByTestId('accordion-header-TransformComponent');
        fireEvent.click(header);
        const body = screen.getByTestId('accordion-body-TransformComponent');
        expect(body).toHaveStyle({ display: 'none' });
    });

    it('clicking header again toggles body back visible', () => {
        render(
            <ComponentAccordion component={mockComponent} onRemove={vi.fn()} />
        );
        const header = screen.getByTestId('accordion-header-TransformComponent');
        fireEvent.click(header);
        fireEvent.click(header);
        const body = screen.getByTestId('accordion-body-TransformComponent');
        expect(body).not.toHaveStyle({ display: 'none' });
    });

    it('renders children in body slot', () => {
        render(
            <ComponentAccordion component={mockComponent} onRemove={vi.fn()}>
                <div data-testid="child-row">field-row</div>
            </ComponentAccordion>
        );
        expect(screen.getByTestId('child-row')).toBeInTheDocument();
    });

    it('remove button calls onRemove with correct type', () => {
        const onRemove = vi.fn();
        render(
            <ComponentAccordion component={mockComponent} onRemove={onRemove} />
        );
        fireEvent.click(screen.getByTestId('remove-btn-TransformComponent'));
        expect(onRemove).toHaveBeenCalledOnce();
        expect(onRemove).toHaveBeenCalledWith('TransformComponent');
    });

    it('data-testid attributes are correct', () => {
        render(
            <ComponentAccordion component={mockComponent} onRemove={vi.fn()} />
        );
        expect(screen.getByTestId('accordion-TransformComponent')).toBeInTheDocument();
        expect(screen.getByTestId('accordion-header-TransformComponent')).toBeInTheDocument();
        expect(screen.getByTestId('accordion-body-TransformComponent')).toBeInTheDocument();
        expect(screen.getByTestId('remove-btn-TransformComponent')).toBeInTheDocument();
    });
});
