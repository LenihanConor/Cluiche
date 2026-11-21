import { decodeU16, decodeU8 } from './decode';

describe('decode', () => {
    it('decodes empty strings to empty arrays', () => {
        expect(decodeU16('')).toHaveLength(0);
        expect(decodeU8('')).toHaveLength(0);
    });

    it('known answer: [150, 275, 65535] little-endian uint16', () => {
        // bytes 96 00 13 01 FF FF, hand-computed (not produced by the TS generator).
        const v = decodeU16('lgATAf//');
        expect(Array.from(v)).toEqual([150, 275, 65535]);
    });

    it('drops a trailing odd byte for uint16', () => {
        // bytes 01 02 03 -> one u16 (0x0201), trailing byte ignored
        expect(Array.from(decodeU16('AQID'))).toEqual([513]);
        expect(Array.from(decodeU8('AQID'))).toEqual([1, 2, 3]);
    });

    it('treats invalid base64 as empty rather than throwing', () => {
        expect(decodeU16('***not base64***')).toHaveLength(0);
        expect(decodeU8('***')).toHaveLength(0);
    });
});
