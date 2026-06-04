import { describe, it, expect } from 'vitest';
import { deriveExpectedPath, buildNavigateFailedContext } from './navigateFailedUtils';

describe('deriveExpectedPath', () => {
    it('handles source_path ending with forward slash (directory)', () => {
        const result = deriveExpectedPath(
            'diaentitytemplate.more',
            'C:\\GitHub\\Cluiche\\Assets\\CluicheTest\\Global\\Scene/',
            'diaentitytemplate'
        );
        expect(result).toBe('C:\\GitHub\\Cluiche\\Assets\\CluicheTest\\Global\\Scene/more.diaentitytemplatetemplate');
    });

    it('handles source_path ending with backslash (directory)', () => {
        const result = deriveExpectedPath(
            'diaentitytemplate.hero',
            'C:\\GitHub\\Cluiche\\Assets\\Global\\Scene\\',
            'diaentitytemplate'
        );
        expect(result).toBe('C:\\GitHub\\Cluiche\\Assets\\Global\\Scene\\hero.diaentitytemplatetemplate');
    });

    it('handles source_path pointing to a file (strips filename, keeps directory)', () => {
        const result = deriveExpectedPath(
            'diaentitytemplate.player',
            'C:\\GitHub\\Cluiche\\Assets\\Scene/wrong.diaentitytemplatetemplate',
            'diaentitytemplate'
        );
        expect(result).toBe('C:\\GitHub\\Cluiche\\Assets\\Scene/player.diaentitytemplatetemplate');
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
            'diaentitytemplate.level.boss',
            '/assets/entities/',
            'diaentitytemplate'
        );
        expect(result).toBe('/assets/entities/level.boss.diaentitytemplatetemplate');
    });

    it('handles instanceId with no dot prefix', () => {
        const result = deriveExpectedPath(
            'standalone',
            '/assets/',
            'diaentitytemplate'
        );
        expect(result).toBe('/assets/standalone.diaentitytemplatetemplate');
    });

    it('handles empty source_path', () => {
        const result = deriveExpectedPath(
            'diaentitytemplate.test',
            '',
            'diaentitytemplate'
        );
        expect(result).toBe('test.diaentitytemplatetemplate');
    });

    it('handles source_path with no directory separator', () => {
        const result = deriveExpectedPath(
            'diaentitytemplate.item',
            'somefile.diaentitytemplatetemplate',
            'diaentitytemplate'
        );
        expect(result).toBe('item.diaentitytemplatetemplate');
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
            assetType: 'diaentitytemplate',
        });
        expect(result).toBeNull();
    });

    it('builds context with correct expectedPath', () => {
        const result = buildNavigateFailedContext({
            instanceId: 'diaentitytemplate.more',
            sourcePath: 'C:\\GitHub\\Cluiche\\Assets\\Scene/',
            error: 'could not open file',
            assetType: 'diaentitytemplate',
        });
        expect(result).not.toBeNull();
        expect(result!.instanceId).toBe('diaentitytemplate.more');
        expect(result!.sourcePath).toBe('C:\\GitHub\\Cluiche\\Assets\\Scene/');
        expect(result!.expectedPath).toBe('C:\\GitHub\\Cluiche\\Assets\\Scene/more.diaentitytemplatetemplate');
        expect(result!.assetType).toBe('diaentitytemplate');
    });

    it('defaults assetType to diaentitytemplate when missing', () => {
        const result = buildNavigateFailedContext({
            instanceId: 'diaentitytemplate.test',
            sourcePath: '/path/',
        });
        expect(result!.assetType).toBe('diaentitytemplate');
    });

    it('handles missing sourcePath gracefully', () => {
        const result = buildNavigateFailedContext({
            instanceId: 'diaentitytemplate.orphan',
        });
        expect(result).not.toBeNull();
        expect(result!.expectedPath).toBe('orphan.diaentitytemplatetemplate');
    });
});
