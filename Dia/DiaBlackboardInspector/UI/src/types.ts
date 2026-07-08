export interface SlotEntry {
    key: string;
    type: string;
    value: unknown;  // JSON value or '[no serializer]'
}

export interface BoardEntry {
    id: string;
    label: string;
    slots: SlotEntry[];
    observers: string[];
}
