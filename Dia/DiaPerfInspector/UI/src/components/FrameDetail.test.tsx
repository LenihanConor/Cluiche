import { render, screen } from '@testing-library/react';
import { FrameDetail } from './FrameDetail';
import { useSelectionStore } from '../useSelectionStore';

vi.mock('../bridge', () => ({ queryFrame: vi.fn(), queryWindow: vi.fn() }));

const data = {
    unit: 'Main', frame: 42, unitMs: 12.5, flags: 0,
    modules: [
        { crc: 3, name: 'Zeta', ms: 1.5 },
        { crc: 1, name: 'Alpha', ms: 4.25 },
        { crc: 2, name: 'Mid', ms: 0.5 },
    ],
    sectionsUnavailableReason: 'sections need profiling on', sections: [],
};

beforeEach(() => useSelectionStore.getState().clear());

describe('FrameDetail', () => {
    it('shows a hint when nothing is selected', () => {
        render(<FrameDetail />);
        expect(screen.getByTestId('frame-detail')).toHaveTextContent(/select a frame/i);
    });

    it('lists modules in schedule order with the unit tick', () => {
        useSelectionStore.setState({ selected: { unit: 'Main', index: 42 }, detail: { status: 'ok', data }, stale: null });
        render(<FrameDetail />);
        const rows = screen.getAllByTestId('frame-module');
        expect(rows.map((r) => r.getAttribute('data-module'))).toEqual(['Zeta', 'Alpha', 'Mid']);
        expect(screen.getByTestId('frame-detail')).toHaveTextContent('Main');
        expect(screen.getByTestId('frame-detail')).toHaveTextContent('frame 42');
        expect(screen.getByTestId('frame-unit-tick')).toHaveTextContent('12.50 ms');
    });

    it('says sections are unavailable with the reason', () => {
        useSelectionStore.setState({ selected: { unit: 'Main', index: 42 }, detail: { status: 'ok', data }, stale: null });
        render(<FrameDetail />);
        expect(screen.getByTestId('frame-sections')).toHaveTextContent('sections unavailable: sections need profiling on');
    });

    it('shows an error reply inline without throwing', () => {
        useSelectionStore.setState({
            selected: { unit: 'Main', index: 1 }, detail: { status: 'error', message: 'frame outside retained range' }, stale: null,
        });
        render(<FrameDetail />);
        expect(screen.getByTestId('frame-error')).toHaveTextContent('frame outside retained range');
    });

    it('marks a stale selection', () => {
        useSelectionStore.setState({
            selected: { unit: 'Main', index: 42 }, detail: { status: 'ok', data }, stale: 'unit Main is no longer reported',
        });
        render(<FrameDetail />);
        expect(screen.getByTestId('frame-stale')).toHaveTextContent('no longer reported');
    });
});
