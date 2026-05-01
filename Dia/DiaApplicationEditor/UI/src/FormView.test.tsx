import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { FormView } from './FormView';

describe('FormView – empty config', () => {
    it('shows "No config properties" message', () => {
        render(<FormView config={{}} onChange={vi.fn()} />);
        expect(screen.getByText(/no config properties/i)).toBeInTheDocument();
    });
});

describe('FormView – boolean fields', () => {
    it('renders a checkbox for boolean values', () => {
        render(<FormView config={{ enabled: true }} onChange={vi.fn()} />);
        expect(screen.getByRole('checkbox')).toBeInTheDocument();
    });

    it('checkbox is checked when value is true', () => {
        render(<FormView config={{ enabled: true }} onChange={vi.fn()} />);
        expect(screen.getByRole('checkbox')).toBeChecked();
    });

    it('checkbox is unchecked when value is false', () => {
        render(<FormView config={{ enabled: false }} onChange={vi.fn()} />);
        expect(screen.getByRole('checkbox')).not.toBeChecked();
    });

    it('calls onChange with new boolean when toggled', async () => {
        const onChange = vi.fn();
        render(<FormView config={{ enabled: true }} onChange={onChange} />);
        await userEvent.click(screen.getByRole('checkbox'));
        expect(onChange).toHaveBeenCalledWith('enabled', false);
    });
});

describe('FormView – number fields', () => {
    it('renders a number input for numeric values', () => {
        render(<FormView config={{ count: 5 }} onChange={vi.fn()} />);
        expect(screen.getByRole('spinbutton')).toBeInTheDocument();
    });

    it('input displays the current value', () => {
        render(<FormView config={{ count: 42 }} onChange={vi.fn()} />);
        expect(screen.getByRole('spinbutton')).toHaveValue(42);
    });

    it('calls onChange with parsed float when value changes', () => {
        const onChange = vi.fn();
        render(<FormView config={{ count: 5 }} onChange={onChange} />);
        const input = screen.getByRole('spinbutton');
        fireEvent.change(input, { target: { value: '10' } });
        expect(onChange).toHaveBeenLastCalledWith('count', 10);
    });
});

describe('FormView – string fields', () => {
    it('renders a text input for string values', () => {
        render(<FormView config={{ name: 'hello' }} onChange={vi.fn()} />);
        expect(screen.getByRole('textbox')).toBeInTheDocument();
    });

    it('input displays the current string value', () => {
        render(<FormView config={{ name: 'hello' }} onChange={vi.fn()} />);
        expect(screen.getByRole('textbox')).toHaveValue('hello');
    });

    it('calls onChange with new string value', () => {
        const onChange = vi.fn();
        render(<FormView config={{ name: 'hello' }} onChange={onChange} />);
        const input = screen.getByRole('textbox');
        fireEvent.change(input, { target: { value: 'world' } });
        expect(onChange).toHaveBeenLastCalledWith('name', 'world');
    });
});

describe('FormView – object/array fields', () => {
    it('shows "edit in JSON view" for object values', () => {
        render(<FormView config={{ nested: { a: 1 } }} onChange={vi.fn()} />);
        expect(screen.getByText(/edit in json view/i)).toBeInTheDocument();
    });

    it('shows "edit in JSON view" for array values', () => {
        render(<FormView config={{ items: [1, 2] }} onChange={vi.fn()} />);
        expect(screen.getByText(/edit in json view/i)).toBeInTheDocument();
    });
});

describe('FormView – mixed config', () => {
    it('renders all field types simultaneously', () => {
        render(<FormView config={{ flag: true, count: 3, label: 'x', data: {} }} onChange={vi.fn()} />);
        expect(screen.getByRole('checkbox')).toBeInTheDocument();
        expect(screen.getByRole('spinbutton')).toBeInTheDocument();
        expect(screen.getByRole('textbox')).toBeInTheDocument();
        expect(screen.getByText(/edit in json view/i)).toBeInTheDocument();
    });
});
