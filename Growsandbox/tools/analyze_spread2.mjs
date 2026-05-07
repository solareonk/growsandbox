// Analyze spread-2 sprite layout by detecting grass tufts on cell edges.
// For each cell in dirt block (8×6), determines which edges are "exposed"
// (have grass = green dominant pixels), then derives the bitmask that
// SHOULD render that sprite. Outputs a (col, row) → bitmask map plus
// the inverse map for AUTOTILE_47.
//
// Usage: node analyze_spread2.mjs

import { readFileSync } from "node:fs";
import { inflateSync } from "node:zlib";

const PNG_PATH = "C:/Users/yudhi/Documents/proton-sdk/.worktrees/world-planner/Growsandbox/tools/dirt_block.png";

const buf = readFileSync(PNG_PATH);
let pos = 8;
function readChunk() {
    const len = buf.readUInt32BE(pos); pos += 4;
    const type = buf.slice(pos, pos+4).toString("ascii"); pos += 4;
    const data = buf.slice(pos, pos+len); pos += len;
    pos += 4;
    return { type, data };
}
const ihdr = readChunk();
const W = ihdr.data.readUInt32BE(0);
const H = ihdr.data.readUInt32BE(4);
const colorType = ihdr.data[9];
const ch = colorType === 6 ? 4 : 3;

let idat = Buffer.alloc(0);
while (pos < buf.length) {
    const c = readChunk();
    if (c.type === "IDAT") idat = Buffer.concat([idat, c.data]);
    if (c.type === "IEND") break;
}
const decompressed = inflateSync(idat);
const stride = W * ch;
const pixels = Buffer.alloc(W * H * ch);
for (let y = 0; y < H; y++) {
    decompressed.copy(pixels, y * stride, y * (stride+1) + 1, (y+1) * (stride+1));
}

const TILE = 32;
const COLS = 8, ROWS = 6;

function isGrass(r, g, b, a) {
    if (a < 100) return false;                 // transparent — not grass, just empty
    return g > r + 15 && g > b + 5 && g > 80;  // green-dominant
}

function isOpaque(r, g, b, a) { return a > 100; }

// Detect grass on each edge of a cell. STRIP_THICKNESS pixels deep.
const STRIP = 5;
const THRESHOLD_RATIO = 0.10; // 10% of strip pixels grass = exposed

function analyzeCell(col, row) {
    const x0 = col * TILE, y0 = row * TILE;
    let nGrass = 0, sGrass = 0, wGrass = 0, eGrass = 0;
    let nOpaque = 0, sOpaque = 0, wOpaque = 0, eOpaque = 0;
    let centerOpaque = 0, centerSamples = 0;

    for (let dy = 0; dy < TILE; dy++) {
        for (let dx = 0; dx < TILE; dx++) {
            const i = ((y0+dy) * W + x0+dx) * ch;
            const r = pixels[i], g = pixels[i+1], b = pixels[i+2];
            const a = ch === 4 ? pixels[i+3] : 255;
            const grass = isGrass(r, g, b, a);
            const opaque = isOpaque(r, g, b, a);

            if (dy < STRIP)         { if (grass) nGrass++; if (opaque) nOpaque++; }
            if (dy >= TILE - STRIP) { if (grass) sGrass++; if (opaque) sOpaque++; }
            if (dx < STRIP)         { if (grass) wGrass++; if (opaque) wOpaque++; }
            if (dx >= TILE - STRIP) { if (grass) eGrass++; if (opaque) eOpaque++; }

            if (dx >= 12 && dx < 20 && dy >= 12 && dy < 20) {
                centerSamples++;
                if (opaque) centerOpaque++;
            }
        }
    }

    const stripPx = TILE * STRIP;
    const minGrass = Math.floor(stripPx * THRESHOLD_RATIO);
    return {
        // N=0 means "exposed top" (grass visible OR transparent above)
        N_exposed: nGrass >= minGrass || nOpaque < stripPx * 0.5,
        S_exposed: sGrass >= minGrass || sOpaque < stripPx * 0.5,
        W_exposed: wGrass >= minGrass || wOpaque < stripPx * 0.5,
        E_exposed: eGrass >= minGrass || eOpaque < stripPx * 0.5,
        nGrass, sGrass, wGrass, eGrass,
        nOpaque, sOpaque, wOpaque, eOpaque,
        centerFilled: centerOpaque / centerSamples,
    };
}

console.log("Cell analysis (col, row): N_exposed S_exposed W_exposed E_exposed | grass counts");
console.log("=".repeat(80));

const cellMap = []; // (col, row) → result

for (let row = 0; row < ROWS; row++) {
    for (let col = 0; col < COLS; col++) {
        const r = analyzeCell(col, row);
        cellMap.push({ col, row, ...r });
        const flags =
            (r.N_exposed ? "N" : ".") +
            (r.S_exposed ? "S" : ".") +
            (r.W_exposed ? "W" : ".") +
            (r.E_exposed ? "E" : ".");
        const mask =
            (r.N_exposed ? 0 : 1) |
            (r.S_exposed ? 0 : 2) |
            (r.W_exposed ? 0 : 4) |
            (r.E_exposed ? 0 : 8);
        console.log(
            `(${col},${row}): exposed=${flags} ortho_mask=${mask.toString().padStart(2)} ` +
            `grass[N=${r.nGrass} S=${r.sGrass} W=${r.wGrass} E=${r.eGrass}] ` +
            `center=${r.centerFilled.toFixed(2)}`
        );
    }
    console.log();
}

console.log("Bitmask → (col, row) [first match only, ortho-only]:");
console.log("=".repeat(80));
const byMask = {};
for (const c of cellMap) {
    const mask =
        (c.N_exposed ? 0 : 1) |
        (c.S_exposed ? 0 : 2) |
        (c.W_exposed ? 0 : 4) |
        (c.E_exposed ? 0 : 8);
    if (!byMask[mask]) byMask[mask] = [];
    byMask[mask].push({ col: c.col, row: c.row });
}
for (let m = 0; m < 16; m++) {
    const cells = byMask[m] || [];
    const N = m & 1 ? "N" : ".";
    const S = m & 2 ? "S" : ".";
    const W = m & 4 ? "W" : ".";
    const E = m & 8 ? "E" : ".";
    console.log(
        `mask=${m.toString().padStart(2)} (${N}${S}${W}${E}): ` +
        cells.map(c => `(${c.col},${c.row})`).join(" ")
    );
}
