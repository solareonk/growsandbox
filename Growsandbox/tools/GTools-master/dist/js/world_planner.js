// World Planner — clone of okass.net/growtopia/planner.php auto-tile system.
// Implements makeGrid + getSpread + buildBlock + cascade redraw, per spec in
// docs (project-side). MVP: spreads 1, 3, 5, 6, 7 fully; 2 approximated as 5;
// 4/8/9/10 fall back to static.

const WORLD_W = 100;
const WORLD_H = 60;
const TILE = 32;
const DATA_URL = "assets/planner/planner_data.json";
const PNG_DIR = "assets/planner/";

const state = {
    data: {},                                            // tile name → meta
    imgCache: {},                                        // file → HTMLImageElement
    paintCache: {},                                      // file → { color → HTMLCanvasElement }
    fg: makeLayer(), bg: makeLayer(), water: makeLayer(), glue: makeLayer(),
    selected: null,                                      // tile name (clearName, no prefix/suffix)
    paintColor: "",                                      // "" | "P"|"R"|"Y"|"G"|"A"|"B"|"C"|"V"
    flip: false,                                         // mirror horizontally
    layer: "fg",
    zoom: 1,
    isDown: false,
    isErase: false,
    weather: "",
    worldName: "world",
    rectMode: false,
    rectStart: null,
    cameraMode: false,
    panLast: null,
    debug: false,
};

// Paint tinting RGB multipliers (per-channel, 0..1). Per okass doc: 7 colors.
const PAINT_RGB = {
    red:      [1.00, 0.39, 0.39],
    yellow:   [1.00, 0.94, 0.39],
    green:    [0.39, 0.94, 0.39],
    aqua:     [0.39, 0.94, 0.78],
    blue:     [0.39, 0.59, 0.94],
    purple:   [0.78, 0.39, 0.94],
    charcoal: [0.30, 0.30, 0.35],
};
const COLOR_LETTER = { R:"red", Y:"yellow", G:"green", A:"aqua", B:"blue", P:"purple", C:"charcoal" };

async function getPaintedImage(file, colorName) {
    if (state.paintCache[file]?.[colorName]) return state.paintCache[file][colorName];
    const img = await loadImage(file);
    if (!img) return null;
    const c = document.createElement("canvas");
    c.width = img.width; c.height = img.height;
    const ctx = c.getContext("2d");
    ctx.drawImage(img, 0, 0);
    let id;
    try {
        id = ctx.getImageData(0, 0, c.width, c.height);
    } catch (e) {
        // Tainted canvas (file:// in some browsers) — fall back to untinted image.
        console.warn(`Paint cache disabled (${e.name}); use http://localhost server for paint to work.`);
        return img;
    }
    const [tr, tg, tb] = PAINT_RGB[colorName] || [1, 1, 1];
    const d = id.data;
    for (let i = 0; i < d.length; i += 4) {
        d[i]   = d[i]   * tr;
        d[i+1] = d[i+1] * tg;
        d[i+2] = d[i+2] * tb;
    }
    ctx.putImageData(id, 0, 0);
    if (!state.paintCache[file]) state.paintCache[file] = {};
    state.paintCache[file][colorName] = c;
    return c;
}

function buildPlaceName() {
    if (!state.selected) return null;
    let n = state.selected;
    if (state.paintColor) n = `P${state.paintColor}_${n}`;
    if (state.flip) n = n + "_FL";
    return n;
}

function updateSelectedLabel() {
    const el = document.getElementById("selected");
    if (!el) return;
    if (!state.selected) { el.textContent = "No tile selected"; return; }
    const def = state.data[state.selected];
    const parts = [buildPlaceName()];
    parts.push(`spread ${def?.spread ?? "?"}`);
    if (state.paintColor) parts.push(`paint ${COLOR_LETTER[state.paintColor]}`);
    if (state.flip) parts.push("flipped");
    el.textContent = parts.join(" • ");
}

function makeLayer() {
    const a = new Array(WORLD_H);
    for (let y = 0; y < WORLD_H; y++) a[y] = new Array(WORLD_W).fill(null);
    return a;
}

function $(id) { return document.getElementById(id); }

// ---------- bootstrap ----------
async function init() {
    if (window.__plannerData) {
        state.data = window.__plannerData;
    } else {
        try {
            const res = await fetch(DATA_URL);
            if (!res.ok) throw new Error(res.statusText);
            state.data = await res.json();
        } catch (e) {
            $("info").innerHTML = `Failed to load data.<br>If opened via file://, run a local HTTP server:<br><code style="color:#9cf">python -m http.server 8000</code><br>then open <code>http://localhost:8000/planner.html</code>`;
            return;
        }
    }
    $("info").textContent = `${Object.keys(state.data).length} tiles loaded`;

    setupCanvases();
    drawGrid();
    await preloadImagesForVisible();
    populatePalette();
    populateWeather();
    bindEvents();
    applyWeather();
}

const ALL_CANVAS_IDS = ["bg-canvas", "water-canvas", "fg-canvas", "glue-canvas", "grid-canvas", "cursor-canvas"];

function setupCanvases() {
    const w = WORLD_W * TILE, h = WORLD_H * TILE;
    for (const id of ALL_CANVAS_IDS) {
        const c = $(id);
        c.width = w; c.height = h;
        c.style.width = (w * state.zoom) + "px";
        c.style.height = (h * state.zoom) + "px";
    }
    $("stage-inner").style.width = (w * state.zoom) + "px";
    $("stage-inner").style.height = (h * state.zoom) + "px";
}

function drawGrid() {
    const ctx = $("grid-canvas").getContext("2d");
    ctx.clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
    ctx.strokeStyle = "rgba(255,255,255,0.05)";
    ctx.lineWidth = 1;
    for (let x = 0; x <= WORLD_W; x++) {
        ctx.beginPath();
        ctx.moveTo(x * TILE + 0.5, 0);
        ctx.lineTo(x * TILE + 0.5, WORLD_H * TILE);
        ctx.stroke();
    }
    for (let y = 0; y <= WORLD_H; y++) {
        ctx.beginPath();
        ctx.moveTo(0, y * TILE + 0.5);
        ctx.lineTo(WORLD_W * TILE, y * TILE + 0.5);
        ctx.stroke();
    }
}

async function loadImage(file) {
    if (state.imgCache[file]) return state.imgCache[file];
    return new Promise((resolve) => {
        const img = new Image();
        img.onload = () => { state.imgCache[file] = img; resolve(img); };
        img.onerror = () => { state.imgCache[file] = null; resolve(null); };
        img.src = PNG_DIR + file;
    });
}

async function preloadImagesForVisible() {
    // Preload tiles_page1 first (most common)
    await loadImage("tiles_page1.png");
}

// ---------- name parsing ----------
function deconstructImage(name) {
    if (!name) return null;
    let raw = name;
    let flip = false, paint = false, color = null;
    let clearName = name;
    if (clearName.endsWith("_FL")) { flip = true; clearName = clearName.slice(0, -3); }
    if (/^P[A-Z]_/.test(clearName)) {
        paint = true;
        const c = clearName[1];
        const map = { P:"purple", R:"red", Y:"yellow", G:"green", A:"aqua", B:"blue", C:"cyan", V:"varnish" };
        color = map[c] || null;
        clearName = clearName.slice(3);
    }
    return { raw, clearName, flipName: flip ? clearName + "_FL" : clearName, flip, paint, color };
}

// ---------- core auto-tile ----------
// makeGrid: build 3x3 grid of neighbor matches.
// 0 = empty/different, 1 = same flipName, 2 = blocked (edge / glue)
// layerName: when "water", glue rules are skipped (water flows over glue).
function makeGrid(x, y, layer, target, layerName) {
    const grid = [[0,0,0],[0,1,0],[0,0,0]];
    const tgt = deconstructImage(target);
    const skipGlue = layerName === "water";
    const selfGlue = !skipGlue && state.glue[y][x] !== null;
    for (let row = 0; row < 3; row++) {
        const ny = y - 1 + row;
        for (let col = 0; col < 3; col++) {
            const nx = x - 1 + col;
            if (row === 1 && col === 1) continue;
            if (nx < 0 || nx >= WORLD_W || ny < 0 || ny >= WORLD_H) {
                grid[row][col] = 2;
                continue;
            }
            // Self has glue → block all directions except top neighbor (row=0,col=1)
            if (selfGlue && !(row === 0 && col === 1)) {
                grid[row][col] = 2;
                continue;
            }
            // Glue at neighbor blocks connection
            if (!skipGlue && state.glue[ny][nx] !== null) {
                grid[row][col] = 2;
                continue;
            }
            const n = layer[ny][nx];
            if (!n) continue;
            const nd = deconstructImage(n);
            if (nd.flipName === tgt.flipName) grid[row][col] = 1;
        }
    }
    return grid;
}

// Spread 2 (full 8-arah) lookup table — empirically extracted from okass planner
// by monkey-patching CanvasRenderingContext2D.drawImage and iterating all 256
// 8-bit neighbor permutations through window.buildBlock. Result: 47 unique
// sprites (cell (7, 5) unused). Bit map: N=1 S=2 W=4 E=8 NW=16 NE=32 SW=64 SE=128.
const AUTOTILE_47 = (() => {
    const m = new Map();
    const G = [
        [0, 0, [255]],
        [0, 1, [21, 53, 85, 117, 149, 181, 213, 245]],
        [0, 2, [127]],
        [0, 3, [47]],
        [0, 4, [43, 59, 107, 123]],
        [0, 5, [45, 109, 173, 237]],
        [1, 0, [206, 222, 238, 254]],
        [1, 1, [3, 19, 35, 51, 67, 83, 99, 115, 131, 147, 163, 179, 195, 211, 227, 243]],
        [1, 2, [207]],
        [1, 3, [79]],
        [1, 4, [11, 27, 75, 91]],
        [1, 5, [29, 93, 157, 221]],
        [2, 0, [61, 125, 189, 253]],
        [2, 1, [2, 18, 34, 50, 66, 82, 98, 114, 130, 146, 162, 178, 194, 210, 226, 242]],
        [2, 2, [63]],
        [2, 3, [143]],
        [2, 4, [71, 103, 199, 231]],
        [2, 5, [13, 77, 141, 205]],
        [3, 0, [171, 187, 235, 251]],
        [3, 1, [1, 17, 33, 49, 65, 81, 97, 113, 129, 145, 161, 177, 193, 209, 225, 241]],
        [3, 2, [175]],
        [3, 3, [15]],
        [3, 4, [23, 55, 151, 183]],
        [3, 5, [9, 25, 73, 89, 137, 153, 201, 217]],
        [4, 0, [87, 119, 215, 247]],
        [4, 1, [0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240]],
        [4, 2, [95]],
        [4, 3, [12, 28, 44, 60, 76, 92, 108, 124, 140, 156, 172, 188, 204, 220, 236, 252]],
        [4, 4, [7, 39, 135, 167]],
        [4, 5, [5, 37, 69, 101, 133, 165, 197, 229]],
        [5, 0, [138, 154, 170, 186, 202, 218, 234, 250]],
        [5, 1, [239]],
        [5, 2, [111]],
        [5, 3, [8, 24, 40, 56, 72, 88, 104, 120, 136, 152, 168, 184, 200, 216, 232, 248]],
        [5, 4, [142, 158, 174, 190]],
        [5, 5, [10, 26, 42, 58, 74, 90, 106, 122]],
        [6, 0, [70, 86, 102, 118, 198, 214, 230, 246]],
        [6, 1, [223]],
        [6, 2, [159]],
        [6, 3, [4, 20, 36, 52, 68, 84, 100, 116, 132, 148, 164, 180, 196, 212, 228, 244]],
        [6, 4, [78, 94, 110, 126]],
        [6, 5, [6, 22, 38, 54, 134, 150, 166, 182]],
        [7, 0, [41, 57, 105, 121, 169, 185, 233, 249]],
        [7, 1, [191]],
        [7, 2, [31]],
        [7, 3, [139, 155, 203, 219]],
        [7, 4, [14, 30, 46, 62]],
    ];
    for (const [x, y, masks] of G) for (const k of masks) m.set(k, { x, y });
    return m;
})();

function getSpread2(g) {
    const N = !!g[0][1], S = !!g[2][1], W = !!g[1][0], E = !!g[1][2];
    const NW = !!g[0][0] && N && W;
    const NE = !!g[0][2] && N && E;
    const SW = !!g[2][0] && S && W;
    const SE = !!g[2][2] && S && E;
    const mask = (N?1:0)|(S?2:0)|(W?4:0)|(E?8:0)|(NW?16:0)|(NE?32:0)|(SW?64:0)|(SE?128:0);
    return AUTOTILE_47.get(mask) || { x: 4, y: 1 };
}

// Spread 4/9 — anti-same: anchors on solid neighbors of a different kind.
// Per spec: solid (spread=1) anchors in any direction; liquid (spread=2)
// anchors only when positioned south of self (spike rises from water below).
function getSpread4(x, y, layer, target, spread) {
    const tgt = deconstructImage(target);
    const anchorAt = (nx, ny, isSouth) => {
        if (nx < 0 || nx >= WORLD_W || ny < 0 || ny >= WORLD_H) return true;
        const n = layer[ny][nx];
        if (!n) return false;
        const nd = deconstructImage(n);
        if (nd.flipName === tgt.flipName) return false;
        const def = state.data[nd.clearName];
        if (!def) return false;
        if (def.spread === 1) return true;
        if (def.spread === 2) return isSouth;
        return false;
    };
    const T = anchorAt(x, y - 1, false);
    const B = anchorAt(x, y + 1, true);
    const L = anchorAt(x - 1, y, false);
    const R = anchorAt(x + 1, y, false);
    if (B) return { x: 3, y: 0 }; // anchor below — spike points up from floor
    if (T) return { x: 1, y: 0 }; // anchor above — spike hangs from ceiling
    if (L) return { x: 0, y: 0 }; // anchor left
    if (R) return { x: 2, y: 0 }; // anchor right
    return { x: spread === 9 ? 3 : 4, y: 0 }; // floating
}

// getSpread dispatcher
function getSpread(spread, g, x, y, layer, target) {
    const T = !!g[0][1], B = !!g[2][1];
    const L = !!g[1][0], R = !!g[1][2];

    switch (spread) {
        case 1: return { x: 0, y: 0 };

        case 3: case 8: {
            if (L && !R) return { x: 2, y: 0 };
            if (!L && R) return { x: 0, y: 0 };
            if (L && R)  return { x: 1, y: 0 };
            return { x: 3, y: 0 };
        }

        case 7: {
            if (T && !B) return { x: 0, y: 0 };
            if (!T && B) return { x: 2, y: 0 };
            if (T && B)  return { x: 1, y: 0 };
            return { x: 3, y: 0 };
        }

        case 5: {
            // Lava-style 4-arah, sprite grid 8×2. Order matches okass spec.
            if (T && L && R && B) return { x: 0, y: 0 };
            if (L && R && B)      return { x: 1, y: 0 };
            if (T && L && R)      return { x: 2, y: 0 };
            if (T && R && B)      return { x: 3, y: 0 };
            if (T && L && B)      return { x: 4, y: 0 };
            if (R && B)           return { x: 5, y: 0 };
            if (L && B)           return { x: 6, y: 0 };
            if (T && R)           return { x: 7, y: 0 };
            if (T && L)           return { x: 0, y: 1 };
            if (T && B)           return { x: 1, y: 1 };
            if (B)                return { x: 2, y: 1 };
            if (T)                return { x: 3, y: 1 };
            if (L && R)           return { x: 5, y: 1 };
            if (R)                return { x: 6, y: 1 };
            if (L)                return { x: 7, y: 1 };
            return { x: 4, y: 1 };
        }

        case 2: return getSpread2(g);

        case 6: return { x: Math.floor(Math.random() * 4), y: 0 };

        case 4: case 9: return getSpread4(x, y, layer, target, spread);

        default: return { x: 0, y: 0 };
    }
}

// ---------- rendering ----------
function getLayerArray(name) {
    return name === "fg" ? state.fg :
           name === "bg" ? state.bg :
           name === "water" ? state.water :
           state.glue;
}
function getLayerCanvas(name) {
    return $(name + "-canvas");
}

// ---------- save / load .gtworld ----------
function serializeLayer(layer) {
    const rows = [];
    for (let y = 0; y < WORLD_H; y++) {
        rows.push(layer[y].map(t => t || "").join(","));
    }
    return rows.join("\n");
}

function saveWorld() {
    let out = "%cernworldplanner;\n";
    out += "weather=" + state.weather + "\n";
    out += "fg=" + serializeLayer(state.fg) + "\n";
    out += "bg=" + serializeLayer(state.bg) + "\n";
    out += "water=" + serializeLayer(state.water) + "\n";
    out += "glue=" + serializeLayer(state.glue);
    const blob = new Blob([out], { type: "application/octet-stream" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = (state.worldName || "world") + ".gtworld";
    a.click();
    URL.revokeObjectURL(url);
}

function parseLayer(text, layerArr) {
    const rows = text.split("\n");
    for (let y = 0; y < WORLD_H; y++) {
        if (!rows[y]) { layerArr[y].fill(null); continue; }
        const cells = rows[y].split(",");
        for (let x = 0; x < WORLD_W; x++) {
            let v = cells[x];
            if (!v) { layerArr[y][x] = null; continue; }
            v = v.replace(/;/g, ",");
            const meta = deconstructImage(v);
            if (!state.data[meta.clearName]) { layerArr[y][x] = null; continue; }
            layerArr[y][x] = v;
        }
    }
}

async function loadWorld(file) {
    const text = await file.text();
    if (!text.startsWith("%cernworldplanner")) {
        alert("Not a valid .gtworld file (missing signature)");
        return;
    }
    state.weather = (text.split("weather=")[1] || "sunny").split("\n")[0].trim();
    const fgPart    = text.split("\nfg=")[1]?.split("\nbg=")[0] || "";
    const bgPart    = text.split("\nbg=")[1]?.split("\nwater=")[0] || "";
    const waterPart = text.split("\nwater=")[1]?.split("\nglue=")[0] || "";
    const gluePart  = text.split("\nglue=")[1] || "";
    parseLayer(fgPart, state.fg);
    parseLayer(bgPart, state.bg);
    parseLayer(waterPart, state.water);
    parseLayer(gluePart, state.glue);
    for (const ln of ["fg", "bg", "water", "glue"]) {
        getLayerCanvas(ln).getContext("2d").clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
        await redrawAll(ln);
    }
    state.worldName = (file.name || "world").replace(/\.gtworld$/i, "");
    const wn = $("worldName"); if (wn) wn.value = state.worldName;
    $("info").textContent = `Loaded ${state.worldName} • weather=${state.weather}`;
}

// ---------- render export ----------
function renderWorldPng() {
    const w = WORLD_W * TILE, h = WORLD_H * TILE;
    const c = document.createElement("canvas");
    c.width = w; c.height = h;
    const ctx = c.getContext("2d");
    ctx.fillStyle = "#1a1a2e";
    ctx.fillRect(0, 0, w, h);
    ctx.drawImage(getLayerCanvas("bg"), 0, 0);
    ctx.globalAlpha = 0.8;
    ctx.drawImage(getLayerCanvas("water"), 0, 0);
    ctx.globalAlpha = 1.0;
    ctx.drawImage(getLayerCanvas("fg"), 0, 0);
    c.toBlob(blob => {
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = (state.worldName || "world") + "_render.png";
        a.click();
        URL.revokeObjectURL(url);
    }, "image/png");
}

// ---------- rectangle fill tool ----------
function clearRectPreview() {
    $("cursor-canvas").getContext("2d").clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
}

function drawRectPreview(x0, y0, x1, y1) {
    const ctx = $("cursor-canvas").getContext("2d");
    ctx.clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
    if (x1 === undefined || x1 === null) {
        ctx.fillStyle = "rgba(255, 200, 0, 0.35)";
        ctx.fillRect(x0 * TILE, y0 * TILE, TILE, TILE);
        ctx.strokeStyle = "#ffc800";
        ctx.lineWidth = 2;
        ctx.strokeRect(x0 * TILE + 1, y0 * TILE + 1, TILE - 2, TILE - 2);
        return;
    }
    const xa = Math.min(x0, x1), xb = Math.max(x0, x1);
    const ya = Math.min(y0, y1), yb = Math.max(y0, y1);
    ctx.fillStyle = "rgba(255, 200, 0, 0.18)";
    ctx.fillRect(xa * TILE, ya * TILE, (xb - xa + 1) * TILE, (yb - ya + 1) * TILE);
    ctx.strokeStyle = "#ffc800";
    ctx.lineWidth = 2;
    ctx.strokeRect(xa * TILE + 1, ya * TILE + 1, (xb - xa + 1) * TILE - 2, (yb - ya + 1) * TILE - 2);
}

async function fillRectangle(x0, y0, x1, y1, erase) {
    const xa = Math.max(0, Math.min(x0, x1));
    const xb = Math.min(WORLD_W - 1, Math.max(x0, x1));
    const ya = Math.max(0, Math.min(y0, y1));
    const yb = Math.min(WORLD_H - 1, Math.max(y0, y1));
    const layer = getLayerArray(state.layer);
    const name = erase ? null : buildPlaceName();
    if (!erase && !name) return;
    for (let y = ya; y <= yb; y++) {
        for (let x = xa; x <= xb; x++) layer[y][x] = name;
    }
    // Redraw rect interior + 1-cell border for cascade neighbors
    const tasks = [];
    for (let y = ya - 1; y <= yb + 1; y++) {
        for (let x = xa - 1; x <= xb + 1; x++) {
            if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) continue;
            tasks.push(drawTile(state.layer, x, y));
            if (state.layer === "glue") {
                tasks.push(drawTile("fg", x, y));
                tasks.push(drawTile("bg", x, y));
                tasks.push(drawTile("water", x, y));
            }
        }
    }
    await Promise.all(tasks);
}

// ---------- count blocks ----------
function countBlocks() {
    const buckets = { fg: {}, bg: {}, water: {}, glue: {}, paint: {} };
    const layers = { fg: state.fg, bg: state.bg, water: state.water, glue: state.glue };
    for (const [k, layer] of Object.entries(layers)) {
        for (let y = 0; y < WORLD_H; y++) {
            for (let x = 0; x < WORLD_W; x++) {
                const t = layer[y][x];
                if (!t) continue;
                const meta = deconstructImage(t);
                if (meta.paint) buckets.paint[meta.color] = (buckets.paint[meta.color] || 0) + 1;
                buckets[k][meta.clearName] = (buckets[k][meta.clearName] || 0) + 1;
            }
        }
    }
    let report = "Required items to build this world.\n\n";
    let total = 0;
    for (const [k, label] of [["fg", "FOREGROUND"], ["bg", "BACKGROUND"], ["water", "WATER"], ["glue", "GLUE"]]) {
        const obj = buckets[k];
        const keys = Object.keys(obj).sort((a, b) => obj[b] - obj[a]);
        if (keys.length === 0) continue;
        report += label + "\n=====\n";
        for (const name of keys) { report += "  " + name + ": " + obj[name] + "\n"; total += obj[name]; }
        report += "\n";
    }
    if (Object.keys(buckets.paint).length > 0) {
        report += "MISC\n=====\n";
        for (const c of Object.keys(buckets.paint).sort()) report += "  Paint Bucket - " + c + ": " + buckets.paint[c] + "\n";
    }
    if (total === 0) report += "(empty world)";
    else report += "\nTotal placed: " + total;
    alert(report);
}

// ---------- weather ----------
const WEATHERS = [
    ["sunny", "Sunny"], ["beach", "Beach Blast"], ["night", "Night"], ["arid", "Arid"],
    ["rainy", "Rainy"], ["harvest", "Harvest Blast"], ["mars", "Mars Blast"], ["spooky", "Spooky"],
    ["nothing", "Nothingness"], ["snowy", "Snowy"], ["undersea", "Undersea Blast"], ["warp", "Warp"],
    ["comet", "Comet"], ["party", "Party"], ["19_pineapple", "Pineapples"], ["snowy_night", "Snowy Night"],
    ["spring", "Spring"], ["howling_sky", "Howling Sky"], ["pagoda", "Pagoda"], ["31_apocalypse", "Apocalypse"],
    ["jungle", "Jungle"], ["balloon", "Balloon Warz"], ["35_autumn", "Autumn"], ["36_love", "Valentine's"],
    ["37_st_patricks", "St. Paddy's"], ["38_epoch_iceberg", "Epoch Iceberg"], ["39_epoch_lava", "Epoch Lava"],
    ["40_epoch_skylands", "Epoch Skylands"], ["42_hacker", "Digital Rain"], ["43_monochrome", "Monochromatic"],
    ["44_treasure", "Treasure Blast"], ["45_hospital", "Hospital Blast"], ["46_bontiful", "Bountiful Blast"],
    ["47_shooting_stars", "Meteor Shower"], ["48_stargaze", "Stargazing"], ["49_landed_ship", "Lactus Landed"],
    ["50_failed_landing", "Lactus Failed"],
];

function populateWeather() {
    const sel = $("weatherSelect");
    for (const [val, label] of WEATHERS) {
        const o = document.createElement("option");
        o.value = val; o.textContent = label;
        sel.appendChild(o);
    }
}

function applyWeather() {
    const stage = $("stage");
    if (state.weather) {
        stage.style.backgroundImage = `url(assets/planner/weathers/${state.weather}.png)`;
        stage.style.backgroundSize = "cover";
        stage.style.backgroundRepeat = "no-repeat";
        stage.style.backgroundAttachment = "local";
    } else {
        stage.style.backgroundImage = "";
    }
    const sel = $("weatherSelect");
    if (sel) sel.value = state.weather || "";
}

async function drawTile(layerName, x, y) {
    const layer = getLayerArray(layerName);
    const ctx = getLayerCanvas(layerName).getContext("2d");
    ctx.clearRect(x * TILE, y * TILE, TILE, TILE);
    const tile = layer[y][x];
    if (!tile) return;
    const meta = deconstructImage(tile);
    const def = state.data[meta.clearName];
    if (!def) return;
    const src = meta.paint
        ? await getPaintedImage(def.file, meta.color)
        : await loadImage(def.file);
    if (!src) return;
    const grid = makeGrid(x, y, layer, tile, layerName);
    const off = getSpread(def.spread, grid, x, y, layer, tile);
    const sx = (def.x + off.x) * TILE;
    const sy = (def.y + off.y) * TILE;
    if (meta.flip) {
        ctx.save();
        ctx.scale(-1, 1);
        ctx.drawImage(src, sx, sy, TILE, TILE, -(x + 1) * TILE, y * TILE, TILE, TILE);
        ctx.restore();
    } else {
        ctx.drawImage(src, sx, sy, TILE, TILE, x * TILE, y * TILE, TILE, TILE);
    }
}

async function buildBlock(x, y, layerName, place, tileName) {
    const layer = getLayerArray(layerName);
    layer[y][x] = place ? tileName : null;
    const tasks = [];
    for (let dy = -1; dy <= 1; dy++) {
        for (let dx = -1; dx <= 1; dx++) {
            const nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= WORLD_W || ny < 0 || ny >= WORLD_H) continue;
            tasks.push(drawTile(layerName, nx, ny));
            // When glue changes, fg/bg/water at same 3x3 must re-pick spread (their grid sees glue)
            if (layerName === "glue") {
                tasks.push(drawTile("fg", nx, ny));
                tasks.push(drawTile("bg", nx, ny));
                tasks.push(drawTile("water", nx, ny));
            }
        }
    }
    await Promise.all(tasks);
}

async function redrawAll(layerName) {
    const layer = getLayerArray(layerName);
    const ctx = getLayerCanvas(layerName).getContext("2d");
    ctx.clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
    const tasks = [];
    for (let y = 0; y < WORLD_H; y++) {
        for (let x = 0; x < WORLD_W; x++) {
            if (layer[y][x]) tasks.push(drawTile(layerName, x, y));
        }
    }
    await Promise.all(tasks);
}

// ---------- palette ----------
function populatePalette() {
    const palette = $("palette");
    palette.innerHTML = "";
    const search = $("search").value.toLowerCase().trim();
    const spreadFilter = $("spreadFilter").value;

    const entries = Object.entries(state.data);
    let count = 0;
    const MAX = 200; // limit palette render for perf

    for (const [name, def] of entries) {
        if (search && !name.toLowerCase().includes(search)) continue;
        if (spreadFilter !== "all" && def.spread !== parseInt(spreadFilter, 10)) continue;
        if (count >= MAX) break;
        count++;

        const btn = document.createElement("div");
        btn.className = "tile-btn";
        if (state.selected === name) btn.classList.add("active");

        const c = document.createElement("canvas");
        c.width = TILE; c.height = TILE;
        const ctx = c.getContext("2d");
        ctx.fillStyle = "#222"; ctx.fillRect(0, 0, TILE, TILE);
        loadImage(def.file).then(img => {
            if (img) ctx.drawImage(img, def.x * TILE, def.y * TILE, TILE, TILE, 0, 0, TILE, TILE);
        });
        btn.appendChild(c);

        const label = document.createElement("div");
        label.className = "name";
        label.textContent = name;
        label.title = `${name} (spread ${def.spread}, ${def.file})`;
        btn.appendChild(label);

        btn.addEventListener("click", () => {
            state.selected = name;
            updateSelectedLabel();
            populatePalette();
        });
        palette.appendChild(btn);
    }
    if (entries.length > MAX) {
        const note = document.createElement("div");
        note.style.cssText = "grid-column:1/-1;font-size:10px;color:#666;text-align:center;padding:4px;";
        note.textContent = `Showing first ${MAX}. Use search to narrow.`;
        palette.appendChild(note);
    }
}

// ---------- events ----------
function bindEvents() {
    $("search").addEventListener("input", populatePalette);
    $("spreadFilter").addEventListener("change", populatePalette);

    document.querySelectorAll(".layer-btn").forEach(btn => {
        btn.addEventListener("click", () => {
            document.querySelectorAll(".layer-btn").forEach(b => b.classList.remove("active"));
            btn.classList.add("active");
            state.layer = btn.dataset.layer;
        });
    });

    document.querySelectorAll(".paint-btn").forEach(btn => {
        btn.addEventListener("click", () => {
            document.querySelectorAll(".paint-btn").forEach(b => b.classList.remove("active"));
            btn.classList.add("active");
            state.paintColor = btn.dataset.paint;
            updateSelectedLabel();
        });
    });

    $("flipToggle").addEventListener("click", () => {
        state.flip = !state.flip;
        $("flipToggle").classList.toggle("active", state.flip);
        updateSelectedLabel();
    });

    $("debugToggle").addEventListener("click", () => {
        state.debug = !state.debug;
        $("debugToggle").classList.toggle("active", state.debug);
        $("debugBar").style.display = state.debug ? "block" : "none";
    });

    $("zoomIn").addEventListener("click", () => { state.zoom = Math.min(4, state.zoom * 1.5); applyZoom(); });
    $("zoomOut").addEventListener("click", () => { state.zoom = Math.max(0.25, state.zoom / 1.5); applyZoom(); });

    $("clearLayer").addEventListener("click", () => {
        const layer = getLayerArray(state.layer);
        for (let y = 0; y < WORLD_H; y++) layer[y].fill(null);
        getLayerCanvas(state.layer).getContext("2d").clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
    });
    $("clearAll").addEventListener("click", () => {
        for (const ln of ["fg", "bg", "water", "glue"]) {
            const layer = getLayerArray(ln);
            for (let y = 0; y < WORLD_H; y++) layer[y].fill(null);
            getLayerCanvas(ln).getContext("2d").clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
        }
    });

    $("worldName").addEventListener("input", e => { state.worldName = e.target.value || "world"; });
    $("saveBtn").addEventListener("click", saveWorld);
    $("loadBtn").addEventListener("click", () => $("loadFile").click());
    $("loadFile").addEventListener("change", e => {
        if (e.target.files && e.target.files[0]) loadWorld(e.target.files[0]);
        e.target.value = "";
    });
    $("renderBtn").addEventListener("click", renderWorldPng);

    $("rectBtn").addEventListener("click", () => {
        state.rectMode = !state.rectMode;
        state.rectStart = null;
        $("rectBtn").classList.toggle("active", state.rectMode);
        clearRectPreview();
    });
    $("cameraBtn").addEventListener("click", () => {
        state.cameraMode = !state.cameraMode;
        $("cameraBtn").classList.toggle("active", state.cameraMode);
        $("cursor-canvas").style.cursor = state.cameraMode ? "grab" : "default";
    });
    $("countBtn").addEventListener("click", countBlocks);
    $("weatherSelect").addEventListener("change", e => {
        state.weather = e.target.value;
        applyWeather();
    });

    const stage = $("stage");
    const top = $("cursor-canvas");
    top.style.pointerEvents = "auto";
    top.addEventListener("mousedown", onDown);
    top.addEventListener("mousemove", onMove);
    top.addEventListener("mouseup", onUp);
    top.addEventListener("mouseleave", onUp);
    top.addEventListener("contextmenu", e => e.preventDefault());
}

function applyZoom() {
    $("zoomLabel").textContent = state.zoom + "×";
    const w = WORLD_W * TILE * state.zoom, h = WORLD_H * TILE * state.zoom;
    for (const id of ALL_CANVAS_IDS) {
        const c = $(id);
        c.style.width = w + "px"; c.style.height = h + "px";
    }
    $("stage-inner").style.width = w + "px";
    $("stage-inner").style.height = h + "px";
}

function eventToTile(e) {
    const rect = $("cursor-canvas").getBoundingClientRect();
    const px = (e.clientX - rect.left) / state.zoom;
    const py = (e.clientY - rect.top) / state.zoom;
    return { x: Math.floor(px / TILE), y: Math.floor(py / TILE) };
}

function onDown(e) {
    if (state.cameraMode) {
        state.isDown = true;
        state.panLast = { x: e.clientX, y: e.clientY };
        $("cursor-canvas").style.cursor = "grabbing";
        return;
    }
    const { x, y } = eventToTile(e);
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return;
    state.isDown = true;
    state.isErase = (e.button === 2);

    if (state.rectMode) {
        if (!state.rectStart) {
            state.rectStart = { x, y, erase: state.isErase };
            drawRectPreview(x, y);
        } else {
            const start = state.rectStart;
            const erase = start.erase || state.isErase;
            state.rectStart = null;
            state.rectMode = false;
            $("rectBtn").classList.remove("active");
            clearRectPreview();
            fillRectangle(start.x, start.y, x, y, erase);
        }
        return;
    }

    if (state.isErase) {
        buildBlock(x, y, state.layer, false, null);
    } else {
        const name = buildPlaceName();
        if (name) buildBlock(x, y, state.layer, true, name);
    }
}

function onMove(e) {
    if (state.cameraMode && state.isDown && state.panLast) {
        const stage = $("stage");
        stage.scrollLeft -= (e.clientX - state.panLast.x);
        stage.scrollTop  -= (e.clientY - state.panLast.y);
        state.panLast = { x: e.clientX, y: e.clientY };
        return;
    }
    const { x, y } = eventToTile(e);
    $("cursor").textContent = `${x}, ${y}`;

    if (state.rectMode) {
        if (state.rectStart) drawRectPreview(state.rectStart.x, state.rectStart.y, x, y);
        else drawRectPreview(x, y);
        return;
    }

    drawCursor(x, y);
    if (state.debug) updateDebugBar(x, y);
    if (!state.isDown) return;
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return;
    if (state.isErase) {
        buildBlock(x, y, state.layer, false, null);
    } else {
        const name = buildPlaceName();
        if (name) buildBlock(x, y, state.layer, true, name);
    }
}

function updateDebugBar(x, y) {
    const bar = $("debugBar");
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) { bar.textContent = "out of bounds"; return; }
    const layer = getLayerArray(state.layer);
    const tile = layer[y][x];
    if (!tile) { bar.textContent = `(${x},${y}) empty on ${state.layer}`; return; }
    const meta = deconstructImage(tile);
    const def = state.data[meta.clearName];
    if (!def) { bar.textContent = `(${x},${y}) ${tile} — no def`; return; }
    const grid = makeGrid(x, y, layer, tile, state.layer);
    const off = getSpread(def.spread, grid, x, y, layer, tile);
    const N = !!grid[0][1], S = !!grid[2][1], W = !!grid[1][0], E = !!grid[1][2];
    const NW = grid[0][0] === 1 && N && W;
    const NE = grid[0][2] === 1 && N && E;
    const SW = grid[2][0] === 1 && S && W;
    const SE = grid[2][2] === 1 && S && E;
    const mask = (N?1:0)|(S?2:0)|(W?4:0)|(E?8:0)|(NW?16:0)|(NE?32:0)|(SW?64:0)|(SE?128:0);
    const sym = (v) => v === 1 ? "#" : v === 2 ? "x" : ".";
    const gridArt = grid.map(r => r.map(sym).join("")).join("\n");
    const dirs = `N=${+N} S=${+S} W=${+W} E=${+E} | NW=${+NW} NE=${+NE} SW=${+SW} SE=${+SE}`;
    bar.textContent =
        `(${x},${y}) ${tile} • spread=${def.spread} • base=(${def.x},${def.y}) → sprite offset=(${off.x},${off.y})\n` +
        `${dirs}\n` +
        `bitmask=${mask} (binary ${mask.toString(2).padStart(8, "0")})\n` +
        `grid (#=same, x=blocked, .=empty):\n${gridArt}`;
}

function onUp() {
    state.isDown = false;
    state.isErase = false;
    state.panLast = null;
    if (state.cameraMode) $("cursor-canvas").style.cursor = "grab";
}

function drawCursor(x, y) {
    const ctx = $("cursor-canvas").getContext("2d");
    ctx.clearRect(0, 0, WORLD_W * TILE, WORLD_H * TILE);
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return;
    ctx.strokeStyle = "rgba(0,255,128,0.8)";
    ctx.lineWidth = 2;
    ctx.strokeRect(x * TILE + 1, y * TILE + 1, TILE - 2, TILE - 2);
}

init();
