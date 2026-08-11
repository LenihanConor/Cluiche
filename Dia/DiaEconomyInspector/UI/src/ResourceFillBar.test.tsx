import { render, screen } from '@testing-library/react';
import { ResourceFillBar } from './ResourceFillBar';

describe('ResourceFillBar', () => {
    it('renders fill width proportional to value/max', () => {
        render(<ResourceFillBar value={50} max={100} type="base" />);
        const inner = screen.getByTestId('fill-bar-inner');
        expect(inner).toHaveStyle({ width: '50%' });
    });

    it('value at max sets fill to 100%', () => {
        render(<ResourceFillBar value={100} max={100} type="base" />);
        const inner = screen.getByTestId('fill-bar-inner');
        expect(inner).toHaveStyle({ width: '100%' });
    });

    it('value=0 sets fill to 0%', () => {
        render(<ResourceFillBar value={0} max={100} type="base" />);
        const inner = screen.getByTestId('fill-bar-inner');
        expect(inner).toHaveStyle({ width: '0%' });
    });

    it('derived type has different fill color from base', () => {
        const { rerender } = render(<ResourceFillBar value={50} max={100} type="base" />);
        const baseInner = screen.getByTestId('fill-bar-inner');
        const baseColor = (baseInner as HTMLElement).style.background;

        rerender(<ResourceFillBar value={50} max={100} type="derived" />);
        const derivedInner = screen.getByTestId('fill-bar-inner');
        const derivedColor = (derivedInner as HTMLElement).style.background;

        expect(derivedColor).not.toBe(baseColor);
    });

    it('base type at max uses warning color', () => {
        render(<ResourceFillBar value={100} max={100} type="base" />);
        const inner = screen.getByTestId('fill-bar-inner');
        // warning #cca700 → rgb(204, 167, 0)
        expect((inner as HTMLElement).style.background).toMatch(/rgb\(204,\s*167,\s*0\)/);
    });

    it('base type at 0 uses error color', () => {
        render(<ResourceFillBar value={0} max={100} type="base" />);
        const inner = screen.getByTestId('fill-bar-inner');
        // error #f48771 → rgb(244, 135, 113)
        expect((inner as HTMLElement).style.background).toMatch(/rgb\(244,\s*135,\s*113\)/);
    });

    it('renders fill-bar container', () => {
        render(<ResourceFillBar value={25} max={100} type="base" />);
        expect(screen.getByTestId('fill-bar')).toBeInTheDocument();
    });

    it('max=0 gives fill of 0%', () => {
        render(<ResourceFillBar value={10} max={0} type="base" />);
        const inner = screen.getByTestId('fill-bar-inner');
        expect(inner).toHaveStyle({ width: '0%' });
    });
});
