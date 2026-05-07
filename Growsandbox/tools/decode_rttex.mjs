// Standalone Node.js port of rtconverter.js process_rttex_to_png.
// Logic mirrors Growsandbox/tools/GTools-master/dist/js/rtconverter.js (line 234).
//
// Usage:
//   node decode_rttex.mjs <input.rttex> <output.png>
//   node decode_rttex.mjs --batch <input dir> <output dir>

import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from "node:fs";
import { inflateSync, deflateSync } from "node:zlib";
import { join, basename, extname } from "node:path";

const args = process.argv.slice(2);

// CRC32 table (PNG)
const CRC_TABLE = new Uint32Array(256);
for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    CRC_TABLE[n] = c >>> 0;
}
function crc32(buf) {
    let c = 0xffffffff;
    for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
}

function chunk(type, data) {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
    const typeBuf = Buffer.from(type, "ascii");
    const crcBuf = Buffer.alloc(4);
    crcBuf.writeUInt32BE(crc32(Buffer.concat([typeBuf, data])), 0);
    return Buffer.concat([len, typeBuf, data, crcBuf]);
}

function encodePng(width, height, hasAlpha, pixels) {
    const channels = hasAlpha ? 4 : 3;
    const colorType = hasAlpha ? 6 : 2;
    const stride = width * channels;
    // Add filter byte (0 = None) per scanline
    const filtered = Buffer.alloc(height * (stride + 1));
    for (let y = 0; y < height; y++) {
        filtered[y * (stride + 1)] = 0;
        pixels.copy(filtered, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
    }
    const compressed = deflateSync(filtered);

    const sig = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(width, 0);
    ihdr.writeUInt32BE(height, 4);
    ihdr[8] = 8;          // bit depth
    ihdr[9] = colorType;
    ihdr[10] = 0;         // compression
    ihdr[11] = 0;         // filter
    ihdr[12] = 0;         // interlace
    return Buffer.concat([
        sig,
        chunk("IHDR", ihdr),
        chunk("IDAT", compressed),
        chunk("IEND", Buffer.alloc(0)),
    ]);
}

function flipVertical(pixels, width, height, channels) {
    const stride = width * channels;
    const out = Buffer.alloc(pixels.length);
    for (let y = 0; y < height; y++) {
        pixels.copy(out, (height - 1 - y) * stride, y * stride, (y + 1) * stride);
    }
    return out;
}

function rttexToPng(rttexBuf, { flip = true } = {}) {
    let buf = rttexBuf;
    if (buf.slice(0, 6).toString("ascii") === "RTPACK") {
        buf = inflateSync(buf.slice(32));
    }
    if (buf.slice(0, 6).toString("ascii") !== "RTTXTR") {
        throw new Error("not a valid RTTEX (no RTTXTR magic after unwrap)");
    }
    const height = buf.readUInt32LE(8);
    const width = buf.readUInt16LE(12);
    const hasAlpha = buf[0x1c] === 1;
    const channels = hasAlpha ? 4 : 3;
    const pixelStart = 0x7c;
    const pixelLen = width * height * channels;
    if (buf.length < pixelStart + pixelLen) {
        throw new Error(`pixel data truncated: need ${pixelLen} got ${buf.length - pixelStart}`);
    }
    let pixels = Buffer.from(buf.slice(pixelStart, pixelStart + pixelLen));
    if (flip) pixels = flipVertical(pixels, width, height, channels);
    return encodePng(width, height, hasAlpha, pixels);
}

function convertOne(inPath, outPath, opts) {
    const buf = readFileSync(inPath);
    const png = rttexToPng(buf, opts);
    writeFileSync(outPath, png);
    return png.length;
}

if (args[0] === "--batch") {
    const inDir = args[1], outDir = args[2];
    const filter = args[3] ? new RegExp(args[3]) : /\.rttex$/i;
    if (!inDir || !outDir) {
        console.error("Usage: node decode_rttex.mjs --batch <inDir> <outDir> [regex]");
        process.exit(1);
    }
    if (!existsSync(outDir)) mkdirSync(outDir, { recursive: true });
    const files = readdirSync(inDir).filter(f => filter.test(f));
    let ok = 0, fail = 0;
    for (const f of files) {
        const out = join(outDir, basename(f, extname(f)) + ".png");
        try {
            const sz = convertOne(join(inDir, f), out, { flip: true });
            console.error(`  ok: ${f} -> ${basename(out)} (${sz} bytes)`);
            ok++;
        } catch (e) {
            console.error(`  fail: ${f}: ${e.message}`);
            fail++;
        }
    }
    console.error(`done: ${ok} ok, ${fail} fail`);
} else {
    const [inPath, outPath] = args;
    if (!inPath || !outPath) {
        console.error("Usage: node decode_rttex.mjs <input.rttex> <output.png>");
        console.error("       node decode_rttex.mjs --batch <inDir> <outDir> [regex]");
        process.exit(1);
    }
    const sz = convertOne(inPath, outPath, { flip: true });
    console.error(`wrote ${outPath} (${sz} bytes)`);
}
