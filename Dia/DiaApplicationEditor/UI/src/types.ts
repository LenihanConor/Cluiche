// TypeScript shapes mirroring ApplicationManifest JSON (from ManifestSerializer output)

export interface ModuleData {
    instance_id: string;
    type: string;
    phases?: string[];
    dependencies?: string[];
    config?: Record<string, unknown>;
}

export interface PhaseData {
    instance_id: string;
    type: string;
    config?: Record<string, unknown>;
}

export interface TransitionData {
    from: string;
    to: string;
}

export interface ProcessingUnitData {
    instance_id: string;
    type: string;
    frequency_hz?: number;
    dedicated_thread?: boolean;
    initial_phase?: string;
    phases: PhaseData[];
    transitions: TransitionData[];
    modules: ModuleData[];
    config?: Record<string, unknown>;
}

export interface ManifestData {
    version: number;
    processing_units: ProcessingUnitData[];
    imports?: string[];
}
