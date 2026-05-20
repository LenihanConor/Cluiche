import { describe, it, expect } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { AppV2 } from './AppV2';

describe('AppV2', () => {
    it('renders three tab buttons', () => {
        render(<AppV2 />);
        expect(screen.getByText('Graph')).toBeTruthy();
        expect(screen.getByText('Presence')).toBeTruthy();
        expect(screen.getByText('Streams')).toBeTruthy();
    });

    it('shows graph tab content by default', () => {
        render(<AppV2 />);
        expect(screen.getByText(/Graph view/)).toBeTruthy();
    });

    it('switches to presence tab on click', async () => {
        const { getByText } = render(<AppV2 />);
        fireEvent.click(getByText('Presence'));
        expect(screen.getByText(/Presence grid/)).toBeTruthy();
    });
});
