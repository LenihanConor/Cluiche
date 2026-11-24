// Base64 decoding for perf.window arrays. Browser + jsdom safe (atob only, no Buffer).

function bytesOf(b64: string | undefined | null): Uint8Array {
    if (!b64) return new Uint8Array(0);
    try {
        const bin = atob(b64);
        const out = new Uint8Array(bin.length);
        for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
        return out;
    } catch {
        return new Uint8Array(0); // malformed input is treated as "no data"
    }
}

/** Little-endian uint16 values. A trailing odd byte is dropped. Empty or invalid input gives an empty array. */
export function decodeU16(b64: string | undefined | null): Uint16Array {
    const b = bytesOf(b64);
    const n = b.length >> 1;
    const out = new Uint16Array(n);
    for (let i = 0; i < n; i++) out[i] = b[i * 2] | (b[i * 2 + 1] << 8);
    return out;
}

export function decodeU8(b64: string | undefined | null): Uint8Array {
    return bytesOf(b64);
}

/** Little-endian uint32 values. Trailing partial words are dropped. */
export function decodeU32(b64: string | undefined | null): Uint32Array {
    const b = bytesOf(b64);
    const n = b.length >> 2;
    const out = new Uint32Array(n);
    for (let i = 0; i < n; i++) out[i] = (b[i * 4] | (b[i * 4 + 1] << 8) | (b[i * 4 + 2] << 16) | (b[i * 4 + 3] << 24)) >>> 0;
    return out;
}

/**
 * Little-endian uint64 values as doubles (exact up to 2^53; steady-clock ns stay below that for about 104 days of uptime).
 * Trailing partial words are dropped.
 */
export function decodeU64(b64: string | undefined | null): Float64Array {
    const b = bytesOf(b64);
    const n = b.length >> 3;
    const out = new Float64Array(n);
    for (let i = 0; i < n; i++) {
        const o = i * 8;
        const lo = (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
        const hi = (b[o + 4] | (b[o + 5] << 8) | (b[o + 6] << 16) | (b[o + 7] << 24)) >>> 0;
        out[i] = hi * 4294967296 + lo;
    }
    return out;
}
