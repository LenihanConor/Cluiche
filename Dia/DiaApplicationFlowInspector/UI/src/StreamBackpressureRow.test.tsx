import { render, screen } from '@testing-library/react';
import { StreamBackpressureRow } from './StreamBackpressureRow';
import type { StreamState } from './useInspectorStore';

function makeStream(overrides: Partial<StreamState> = {}): StreamState {
    return {
        streamId: 'SimStream',
        msgPerSec: 60,
        kbPerSec: 2.4,
        fillPercent: 30,
        dropsTotal: 0,
        ...overrides,
    };
}

describe('StreamBackpressureRow', () => {
    it('renders stream id', () => {
        render(<StreamBackpressureRow stream={makeStream()} />);
        expect(screen.getByTestId('stream-id').textContent).toBe('SimStream');
    });

    it('bar width matches fillPercent', () => {
        render(<StreamBackpressureRow stream={makeStream({ fillPercent: 45 })} />);
        const bar = screen.getByTestId('fill-bar');
        expect(bar.style.width).toBe('45%');
    });

    it('bar is green when fill < 60%', () => {
        render(<StreamBackpressureRow stream={makeStream({ fillPercent: 40 })} />);
        const bar = screen.getByTestId('fill-bar');
        expect(bar.style.background).toBe('rgb(76, 175, 80)');
    });

    it('bar is amber when fill 60-84%', () => {
        render(<StreamBackpressureRow stream={makeStream({ fillPercent: 70 })} />);
        const bar = screen.getByTestId('fill-bar');
        expect(bar.style.background).toBe('rgb(255, 152, 0)');
    });

    it('bar is red when fill >= 85%', () => {
        render(<StreamBackpressureRow stream={makeStream({ fillPercent: 90 })} />);
        const bar = screen.getByTestId('fill-bar');
        expect(bar.style.background).toBe('rgb(244, 67, 54)');
    });

    it('drops badge is red and bold when drops > 0', () => {
        render(<StreamBackpressureRow stream={makeStream({ dropsTotal: 5 })} />);
        const badge = screen.getByTestId('drops-badge');
        expect(badge.textContent).toBe('5 drops');
        expect(badge.style.color).toBe('rgb(244, 67, 54)');
        expect(badge.style.fontWeight).toBe('700');
    });

    it('drops badge shows 0 drops when no drops', () => {
        render(<StreamBackpressureRow stream={makeStream({ dropsTotal: 0 })} />);
        const badge = screen.getByTestId('drops-badge');
        expect(badge.textContent).toBe('0 drops');
    });
});
