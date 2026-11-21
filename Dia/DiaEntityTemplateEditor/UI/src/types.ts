export interface BlueprintItem {
    id: string;
    label: string;
    path: string;
}

export interface BlueprintGroup {
    label: string;
    items: BlueprintItem[];
}

export interface FieldEntry {
    name: string;
    kind: string;
    value?: string | number | boolean | null;
    codeDefault?: string | number | boolean | null;
    // Optional .diaschema metadata (DiaValueSchema Feature 7); absent in old-format schemas.
    label?: string;
    description?: string;
    unit?: string;
    hint?: string;
    min?: number;
    max?: number;
    default?: number | string | boolean;
}

export interface ComponentEntry {
    type: string;
    fields: FieldEntry[];
}

export interface BlueprintProperties {
    id: string;
    components: ComponentEntry[];
}

export interface UsageEntry {
    sceneId: string;
    instanceCount: number;
}

export interface AvailableComponent {
    typeId: string;
    label: string;
    description?: string;
}

export interface NavigateFailedData {
    instanceId: string;
    sourcePath: string;
    error: string;
    assetType: string;
}
