import React from 'react';
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { NavigateFailedModal } from './NavigateFailedModal';
import type { NavigateFailedData } from '../types';

vi.mock('@dia/editor-ui', () => ({
    theme: { bgPanel: '#252526', border: '#3c3c3c', text: '#d4d4d4', error: '#f48771' },
    buttonStyle: () => ({}),
    buildNavigateFailedContext: vi.fn((data: any) => data?.instanceId ? {
        instanceId: data.instanceId,
        sourcePath: data.sourcePath || '',
        expectedPath: `path/${data.instanceId}.${data.assetType || 'diaentitytemplate'}`,
        assetType: data.assetType || 'diaentitytemplate',
    } : null),
    NavigateFailedContext: undefined,
}));

const sampleData: NavigateFailedData = {
    instanceId: 'hero_knight',
    sourcePath: 'Assets/Blueprints',
    error: 'File not found on disk',
    assetType: 'diaentitytemplate',
};

describe('NavigateFailedModal', () => {
    const onCreateFile = vi.fn();
    const onRemoveEntry = vi.fn();
    const onDismiss = vi.fn();

    beforeEach(() => {
        onCreateFile.mockClear();
        onRemoveEntry.mockClear();
        onDismiss.mockClear();
    });

    it('returns null when data is null', () => {
        const { container } = render(
            <NavigateFailedModal
                data={null}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        expect(container.firstChild).toBeNull();
    });

    it('renders modal when data is provided', () => {
        render(
            <NavigateFailedModal
                data={sampleData}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        expect(screen.getByTestId('navigate-failed-modal')).toBeDefined();
        expect(screen.getByText('Blueprint Not Found')).toBeDefined();
    });

    it('shows instanceId in message', () => {
        render(
            <NavigateFailedModal
                data={sampleData}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        expect(screen.getByText('hero_knight')).toBeDefined();
    });

    it('"Create File" button calls onCreateFile with context', () => {
        render(
            <NavigateFailedModal
                data={sampleData}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        fireEvent.click(screen.getByTestId('create-file-btn'));
        expect(onCreateFile).toHaveBeenCalledTimes(1);
        expect(onCreateFile).toHaveBeenCalledWith({
            instanceId: 'hero_knight',
            sourcePath: 'Assets/Blueprints',
            expectedPath: 'path/hero_knight.diaentitytemplate',
            assetType: 'diaentitytemplate',
        });
    });

    it('"Remove Entry" button calls onRemoveEntry with context', () => {
        render(
            <NavigateFailedModal
                data={sampleData}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        fireEvent.click(screen.getByTestId('remove-entry-btn'));
        expect(onRemoveEntry).toHaveBeenCalledTimes(1);
        expect(onRemoveEntry).toHaveBeenCalledWith({
            instanceId: 'hero_knight',
            sourcePath: 'Assets/Blueprints',
            expectedPath: 'path/hero_knight.diaentitytemplate',
            assetType: 'diaentitytemplate',
        });
    });

    it('"Dismiss" button calls onDismiss', () => {
        render(
            <NavigateFailedModal
                data={sampleData}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        fireEvent.click(screen.getByTestId('dismiss-btn'));
        expect(onDismiss).toHaveBeenCalledTimes(1);
    });

    it('returns null when buildNavigateFailedContext returns null (empty instanceId)', () => {
        const dataWithNoId: NavigateFailedData = {
            instanceId: '',
            sourcePath: 'Assets/Blueprints',
            error: 'File not found',
            assetType: 'diaentitytemplate',
        };
        const { container } = render(
            <NavigateFailedModal
                data={dataWithNoId}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={onDismiss}
            />
        );
        expect(container.firstChild).toBeNull();
    });
});
