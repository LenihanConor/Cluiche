export interface FieldEntry {
    n: string;   // name
    v: string;   // value as string
    t: string;   // type class: 'bool'|'num'|'float'|'vec'|'str'
    dt?: string; // display type string
}

export interface ComponentEntry {
    name: string;
    tier?: string;
    fields: FieldEntry[];
}

export interface EntityEntry {
    i: number;        // index
    g: number;        // gen
    n: string;        // debug name
    t: string[];      // component tags (e.g. ['T','P'])
    d: number;        // hierarchy depth
    pi?: number;      // parent index (-1 = none)
    components?: ComponentEntry[];
}

export interface QueryEntry {
    sig: string;
    count: number;
    members: string[];
}

export interface MailboxEntry {
    f: number;    // frame
    s: string;    // sender
    a: string;    // address
    ak?: string;  // address kind
    type: string;
    tc?: string;  // type class css
    p?: string;   // payload
    inv?: string; // involved entity name
}

export interface WatchItem {
    e: string;
    c: string;
    f: string;
    v?: string | null;
    d?: string;  // 'up'|'dn'|'eq'
}
