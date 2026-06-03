import { describe, it, expect } from 'vitest';
import { deriveExpectedPath, buildNavigateFailedContext } from './navigateFailedUtils';

describe('deriveExpectedPath', () => {
    it('handles source_path ending with forward slash (directory)', () => {
        const result = deriveExpectedPath(
            'diaentity.more',
            'C:\\GitHub\\Cluiche\\Assets\\CluicheTest\\Global\\Scene/',
            'diaentity'
        );
        expect(result).toBe('C:\\GitHub\\Cluiche\\Assets\\CluicheTest\\Global\\Scene/more.diaentity');
    });

    it('handles source_path ending with backslash (directory)', () => {
        const result = deriveExpectedPath(
            'diaentity.hero',
            'C:\\GitHub\\Cluiche\\Assets\\Global\\Scene\\',
            'diaentity'
        );
        expect(result).toBe('C:\\GitHub\\Cluiche\\Assets\\Global\\Scene\\hero.diaentity');
    });

    it('handles source_path pointing to a file (strips filename, keeps directory)', () => {
        const result = deriveExpectedPath(
            'diaentity.player',
            'C:\\GitHub\\Cluiche\\Assets\\Scene/wrong.diaentity',
            'diaentity'
        );
        expect(result).toBe('C:\\GitHub\\Cluiche\\Assets\\Scene/player.diaentity');
    });

    it('handles camera asset type', () => {
        const result = deriveExpectedPath(
            'diacamera.main',
            '/assets/cameras/',
            'diacamera'
        );
        expect(result).toBe('/assets/cameras/main.diacamera');
    });

    it('handles light asset type', () => {
        const result = deriveExpectedPath(
            'dialight.sun',
            '/assets/lights/',
            'dialight'
        );
        expect(result).toBe('/assets/lights/sun.dialight');
    });

    it('handles instanceId with multiple dots (uses text after first dot)', () => {
        const result = deriveExpectedPath(
            'diaentity.level.boss',
            '/assets/entities/',
            'diaentity'
        );
        expect(result).toBe('/assets/entities/level.boss.diaentity');
    });

    it('handles instanceId with no dot prefix', () => {
        const result = deriveExpectedPath(
            'standalone',
            '/assets/',
            'diaentity'
        );
        expect(result).toBe('/assets/standalone.diaentity');
    });

    it('handles empty source_path', () => {
        const result = deriveExpectedPath(
            'diaentity.test',
            '',
            'diaentity'
        );
        expect(result).toBe('test.diaentity');
    });

    it('handles source_path with no directory separator', () => {
        const result = deriveExpectedPath(
            'diaentity.item',
            'somefile.diaentity',
            'diaentity'
        );
        expect(result).toBe('item.diaentity');
    });
});

describe('buildNavigateFailedContext', () => {
    it('returns null for null data', () => {
        expect(buildNavigateFailedContext(null as any)).toBeNull();
    });

    it('returns null for empty instanceId', () => {
        const result = buildNavigateFailedContext({
            instanceId: '',
            sourcePath: '/some/path/',
            assetType: 'diaentity',
        });
        expect(result).toBeNull();
    });

    it('builds context with correct expectedPath', () => {
        const result = buildNavigateFailedContext({
            instanceId: 'diaentity.more',
            sourcePath: 'C:\\GitHub\\Cluiche\\Assets\\Scene/',
            error: 'could not open file',
            assetType: 'diaentity',
        });
        expect(result).not.toBeNull();
        expect(result!.instanceId).toBe('diaentity.more');
        expect(result!.sourcePath).toBe('C:\\GitHub\\Cluiche\\Assets\\Scene/');
        expect(result!.expectedPath).toBe('C:\\GitHub\\Cluiche\\Assets\\Scene/more.diaentity');
        expect(result!.assetType).toBe('diaentity');
    });

    it('defaults assetType to diaentity when missing', () => {
        const result = buildNavigateFailedContext({
            instanceId: 'diaentity.test',
            sourcePath: '/path/',
        });
        expect(result!.assetType).toBe('diaentity');
    });

    it('handles missing sourcePath gracefully', () => {
        const result = buildNavigateFailedContext({
            instanceId: 'diaentity.orphan',
        });
        expect(result).not.toBeNull();
        expect(result!.expectedPath).toBe('orphan.diaentity');
    });
});
