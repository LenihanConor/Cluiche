export type OverflowPolicy = 'drop-oldest' | 'drop-newest' | 'block' | 'fail-loud';

export interface StreamV2 {
    id: string;
    kind: string;
    payloadType: string;
    fromPU: string;
    toPU: string;
    capacity: number;
    maxReaders: number;
    multiWriter?: boolean;
    overflow?: OverflowPolicy;
    blockTimeoutMs?: number;
}

export interface ModuleV2 {
    instanceId: string;
    typeId: string;
    stages: string[];
    dependencies: string[];
    reads: string[];
    writes: string[];
    startTimeoutMs: number;
    stopTimeoutMs: number;
}

export interface ProcessingUnitV2 {
    instanceId: string;
    frequencyHz: number;
    dedicatedThread: boolean;
    modules: ModuleV2[];
}

export interface StageV2 {
    name: string;
    manifestPath: string;
    transitions: string[];
    autoAdvance: boolean;
}

export interface ManifestV2 {
    version: number;
    stages: StageV2[];
    initialStage: string;
    streams: StreamV2[];
    processingUnits: ProcessingUnitV2[];
}

export interface ManifestStateV2 {
    filePath: string;
    isDirty: boolean;
    hasManifest: boolean;
    manifest: ManifestV2 | null;
}
