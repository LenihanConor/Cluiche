export interface NavigateFailedContext {
    instanceId: string;
    sourcePath: string;
    expectedPath: string;
    assetType: string;
}

export function deriveExpectedPath(instanceId: string, sourcePath: string, assetType: string): string {
    let dirPath = sourcePath;
    if (dirPath && dirPath.charAt(dirPath.length - 1) !== '/' && dirPath.charAt(dirPath.length - 1) !== '\\') {
        const lastSlash = Math.max(dirPath.lastIndexOf('/'), dirPath.lastIndexOf('\\'));
        dirPath = lastSlash >= 0 ? dirPath.substring(0, lastSlash + 1) : '';
    }
    const shortName = instanceId.indexOf('.') >= 0 ? instanceId.substring(instanceId.indexOf('.') + 1) : instanceId;
    return dirPath + shortName + '.' + assetType;
}

export function buildNavigateFailedContext(data: {
    instanceId?: string;
    sourcePath?: string;
    error?: string;
    assetType?: string;
}): NavigateFailedContext | null {
    if (!data) return null;
    const instanceId = data.instanceId || '';
    const sourcePath = data.sourcePath || '';
    const assetType = data.assetType || 'diaentitytemplate';

    if (!instanceId) return null;

    const expectedPath = deriveExpectedPath(instanceId, sourcePath, assetType);

    return { instanceId, sourcePath, expectedPath, assetType };
}
