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
