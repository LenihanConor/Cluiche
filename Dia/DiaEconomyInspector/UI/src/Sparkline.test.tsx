import { render, screen } from '@testing-library/react';
import { Sparkline } from './Sparkline';

describe('Sparkline', () => {
    it('renders SVG element', () => {
        render(<Sparkline history={[1, 2, 3]} />);
        expect(screen.getByTestId('sparkline')).toBeInTheDocument();
    });

    it('polyline exists when history has >= 2 points', () => {
        render(<Sparkline history={[10, 20, 15, 30]} />);
        expect(screen.getByTestId('sparkline-line')).toBeInTheDocument();
    });

    it('no polyline when history has < 2 points (empty)', () => {
        render(<Sparkline history={[]} />);
        expect(screen.queryByTestId('sparkline-line')).toBeNull();
    });

    it('no polyline when history has exactly 1 point', () => {
        render(<Sparkline history={[42]} />);
        expect(screen.queryByTestId('sparkline-line')).toBeNull();
    });

    it('polyline points attribute has same count as history length', () => {
        const history = [1, 2, 3, 4, 5];
        render(<Sparkline history={history} />);
        const line = screen.getByTestId('sparkline-line');
        const points = (line.getAttribute('points') ?? '').trim().split(/\s+/);
        expect(points.length).toBe(history.length);
    });

    it('renders with exactly 2 points in history', () => {
        render(<Sparkline history={[5, 10]} />);
        expect(screen.getByTestId('sparkline-line')).toBeInTheDocument();
    });
});
