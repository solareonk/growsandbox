// Standalone Node.js port of items_tools.js item_decoder (TXT mode).
// Reads items.dat → writes items.txt in the GTools format.
// Logic mirrors Growsandbox/tools/GTools-master/dist/js/items_tools.js (lines 602-941).
//
// Usage:  node decode_items.mjs <input items.dat> <output items.txt>

import { readFileSync, writeFileSync } from "node:fs";

const SECRET_KEY = "PBG892FXX982ABC*";

const [, , inPath, outPath] = process.argv;
if (!inPath || !outPath) {
    console.error("Usage: node decode_items.mjs <items.dat> <items.txt>");
    process.exit(1);
}

const buf = readFileSync(inPath);

const FIELD_ORDER = [
    "item_id", "editable_type", "item_category", "action_type", "hit_sound_type",
    "name", "texture", "texture_hash", "item_kind", "val1",
    "texture_x", "texture_y", "spread_type", "is_stripey_wallpaper", "collision_type",
    "break_hits", "drop_chance", "clothing_type", "rarity", "max_amount",
    "extra_file", "extra_file_hash", "audio_volume",
    "pet_name", "pet_prefix", "pet_suffix", "pet_ability",
    "seed_base", "seed_overlay", "tree_base", "tree_leaves",
    "seed_color", "seed_overlay_color",
    "grow_time", "val2", "is_rayman",
    "extra_options", "texture2", "extra_options2",
    "data_position_80", "punch_options",
    "data_version_12", "int_version_13", "int_version_14",
    "data_version_15", "str_version_15", "str_version_16",
    "int_version_17", "int_version_18", "int_version_19",
    "int_version_21", "str_version_22", "int_version_23", "int_version_24",
];

function readNum(b, pos, len) {
    let v = 0;
    for (let i = 0; i < len; i++) v += b[pos + i] << (i * 8);
    return v >>> 0;
}

function readStr(b, pos, len, useKey, itemId) {
    let s = "";
    for (let i = 0; i < len; i++) {
        let c = b[pos + i];
        if (useKey) c = c ^ SECRET_KEY.charCodeAt((i + itemId) % SECRET_KEY.length);
        s += String.fromCharCode(c);
    }
    return s;
}

function readHex(b, pos, len) {
    let s = "";
    for (let i = 0; i < len; i++) s += b[pos + i].toString(16).padStart(2, "0");
    return s.toUpperCase();
}

let pos = 0;
const version = readNum(buf, 0, 2); pos += 2;
const itemCount = readNum(buf, 2, 4); pos += 4;

console.error(`version=${version} item_count=${itemCount}`);

// For v25+: GTools/GTPS haven't reverse-engineered new fields yet.
// We auto-detect total bytes added by scanning for offset where next item_id appears,
// then validate the offset by parsing several items forward and confirming sequential ids.
let unknownTailBytes = -1;

function tryParseSingleItem(buf, startPos, expectedId, version) {
    const SANE_STR = 256;
    let pos = startPos;
    if (pos + 4 > buf.length) return -1;
    if (readNum(buf, pos, 4) !== expectedId) return -1;
    pos += 4 + 4;

    let len;
    len = readNum(buf, pos, 2); if (len > SANE_STR) return -1; pos += 2 + len;
    if (pos > buf.length) return -1;
    len = readNum(buf, pos, 2); if (len > SANE_STR) return -1; pos += 2 + len;
    if (pos > buf.length) return -1;

    pos += 23;

    len = readNum(buf, pos, 2); if (len > SANE_STR) return -1; pos += 2 + len;
    pos += 8;

    for (let i = 0; i < 4; i++) {
        if (pos + 2 > buf.length) return -1;
        len = readNum(buf, pos, 2); if (len > SANE_STR) return -1; pos += 2 + len;
    }

    pos += 24;

    for (let i = 0; i < 3; i++) {
        if (pos + 2 > buf.length) return -1;
        len = readNum(buf, pos, 2); if (len > SANE_STR) return -1; pos += 2 + len;
    }
    pos += 80;

    if (version >= 11) { len = readNum(buf, pos, 2); pos += 2 + len; }
    if (version >= 12) pos += 13;
    if (version >= 13) pos += 4;
    if (version >= 14) pos += 4;
    if (version >= 15) {
        pos += 25;
        len = readNum(buf, pos, 2); pos += 2 + len;
    }
    if (version >= 16) { len = readNum(buf, pos, 2); pos += 2 + len; }
    if (version >= 17) pos += 4;
    if (version >= 18) pos += 4;
    if (version >= 19) pos += 9;
    if (version >= 21) pos += 2;
    if (version >= 22) { len = readNum(buf, pos, 2); pos += 2 + len; }
    if (version >= 23) pos += 4;
    if (version >= 24) pos += 1;

    return pos;
}

function detectTail(buf, posAfterFirstV24, firstItemId, version, itemCount) {
    const validateCount = Math.min(200, itemCount - firstItemId - 1);
    for (let off = 0; off <= 200; off++) {
        let pos = posAfterFirstV24 + off;
        let ok = true;
        for (let k = 1; k <= validateCount; k++) {
            const next = tryParseSingleItem(buf, pos, firstItemId + k, version);
            if (next === -1) { ok = false; break; }
            pos = next + off;
        }
        if (ok) return off;
    }
    return -1;
}

const items = [];

for (let a = 0; a < itemCount; a++) {
    if (pos >= buf.length) {
        console.error(`buffer exhausted at item ${a}/${itemCount}, pos=${pos}, buf.length=${buf.length}`);
        process.exit(4);
    }
    const it = {};
    const itemStartPos = pos;
    it.item_id = readNum(buf, pos, 4); pos += 4;
    it.editable_type = buf[pos++];
    it.item_category = buf[pos++];
    it.action_type = buf[pos++];
    it.hit_sound_type = buf[pos++];

    let len = readNum(buf, pos, 2); pos += 2;
    it.name = readStr(buf, pos, len, true, it.item_id); pos += len;

    len = readNum(buf, pos, 2); pos += 2;
    it.texture = readStr(buf, pos, len, false); pos += len;

    it.texture_hash = readNum(buf, pos, 4); pos += 4;
    it.item_kind = buf[pos++];
    it.val1 = readNum(buf, pos, 4); pos += 4;
    it.texture_x = buf[pos++];
    it.texture_y = buf[pos++];
    it.spread_type = buf[pos++];
    it.is_stripey_wallpaper = buf[pos++];
    it.collision_type = buf[pos++];

    let breakHits = buf[pos++];
    it.break_hits = (breakHits % 6 !== 0) ? `${breakHits}r` : breakHits / 6;

    it.drop_chance = readNum(buf, pos, 4); pos += 4;
    it.clothing_type = buf[pos++];
    it.rarity = readNum(buf, pos, 2); pos += 2;
    it.max_amount = buf[pos++];

    len = readNum(buf, pos, 2); pos += 2;
    it.extra_file = readStr(buf, pos, len, false); pos += len;

    it.extra_file_hash = readNum(buf, pos, 4); pos += 4;
    it.audio_volume = readNum(buf, pos, 4); pos += 4;

    len = readNum(buf, pos, 2); pos += 2;
    it.pet_name = readStr(buf, pos, len, false); pos += len;

    len = readNum(buf, pos, 2); pos += 2;
    it.pet_prefix = readStr(buf, pos, len, false); pos += len;

    len = readNum(buf, pos, 2); pos += 2;
    it.pet_suffix = readStr(buf, pos, len, false); pos += len;

    len = readNum(buf, pos, 2); pos += 2;
    it.pet_ability = readStr(buf, pos, len, false); pos += len;

    it.seed_base = buf[pos++];
    it.seed_overlay = buf[pos++];
    it.tree_base = buf[pos++];
    it.tree_leaves = buf[pos++];

    const sca = buf[pos++], scr = buf[pos++], scg = buf[pos++], scb = buf[pos++];
    const soa = buf[pos++], sor = buf[pos++], sog = buf[pos++], sob = buf[pos++];
    it.seed_color = `${sca},${scr},${scg},${scb}`;
    it.seed_overlay_color = `${soa},${sor},${sog},${sob}`;

    pos += 4; // skip ingredients
    it.grow_time = readNum(buf, pos, 4); pos += 4;
    it.val2 = readNum(buf, pos, 2); pos += 2;
    it.is_rayman = readNum(buf, pos, 2); pos += 2;

    len = readNum(buf, pos, 2); pos += 2;
    it.extra_options = readStr(buf, pos, len, false); pos += len;

    len = readNum(buf, pos, 2); pos += 2;
    it.texture2 = readStr(buf, pos, len, false); pos += len;

    len = readNum(buf, pos, 2); pos += 2;
    it.extra_options2 = readStr(buf, pos, len, false); pos += len;

    if (pos + 80 > buf.length) {
        console.error(`readHex(data_position_80) out of bounds at item ${a}, pos=${pos}, item start=${itemStartPos}, name="${it.name}"`);
        process.exit(5);
    }
    it.data_position_80 = readHex(buf, pos, 80); pos += 80;

    if (version >= 11) {
        len = readNum(buf, pos, 2); pos += 2;
        it.punch_options = readStr(buf, pos, len, false); pos += len;
    }
    if (version >= 12) { it.data_version_12 = readHex(buf, pos, 13); pos += 13; }
    if (version >= 13) { it.int_version_13 = readNum(buf, pos, 4); pos += 4; }
    if (version >= 14) { it.int_version_14 = readNum(buf, pos, 4); pos += 4; }
    if (version >= 15) {
        it.data_version_15 = readHex(buf, pos, 25); pos += 25;
        len = readNum(buf, pos, 2); pos += 2;
        it.str_version_15 = readStr(buf, pos, len, false); pos += len;
    }
    if (version >= 16) {
        len = readNum(buf, pos, 2); pos += 2;
        it.str_version_16 = readStr(buf, pos, len, false); pos += len;
    }
    if (version >= 17) { it.int_version_17 = readNum(buf, pos, 4); pos += 4; }
    if (version >= 18) { it.int_version_18 = readNum(buf, pos, 4); pos += 4; }
    if (version >= 19) { it.int_version_19 = readNum(buf, pos, 9); pos += 9; }
    if (version >= 21) { it.int_version_21 = readNum(buf, pos, 2); pos += 2; }
    if (version >= 22) {
        len = readNum(buf, pos, 2); pos += 2;
        it.str_version_22 = readStr(buf, pos, len, false); pos += len;
    }
    if (version >= 23) { it.int_version_23 = readNum(buf, pos, 4); pos += 4; }
    if (version >= 24) { it.int_version_24 = buf[pos++]; }

    if (version >= 25) {
        if (a === itemCount - 1) {
            pos = buf.length;
        } else {
            const target = a + 1;
            const candidates = [];
            for (let o = 0; o <= 400; o++) {
                if (pos + o + 4 > buf.length) break;
                if (readNum(buf, pos + o, 4) === target) candidates.push(o);
            }
            if (candidates.length === 0) {
                console.error(`no candidate offset for item ${target} after item ${a} (id=${it.item_id}, name="${it.name}")`);
                process.exit(3);
            }
            // Prefer candidate where chain validates; else fall back to first match
            let off = candidates.find(o => tryParseSingleItem(buf, pos + o, target, version) !== -1);
            if (off === undefined) off = candidates[0];
            if (off !== unknownTailBytes && unknownTailBytes !== -1) {
                console.error(`item ${a} (${it.name}): tail offset ${off} bytes (typical ${unknownTailBytes})`);
            }
            unknownTailBytes = off;
            pos += off;
        }
    }

    if (it.item_id !== a) console.error(`Unordered item at index ${a} (id=${it.item_id})`);
    items.push(it);
}

console.error(`parsed ${items.length} items, bytes consumed: ${pos}/${buf.length}`);

const presentFields = FIELD_ORDER.filter(f => f in items[0]);

const header =
    "//Credit: IProgramInCPP & GrowtopiaNoobs\n" +
    "//Format: add_item\\" + presentFields.join("\\") + "\n" +
    "//NOTE: There are several items, for the breakhits part, add 'r'.\n" +
    "//Example: 184r\n" +
    "//What does it mean? So, adding 'r' to breakhits makes it raw breakhits, meaning, " +
    "if you add 'r' to breakhits, when encoding items.dat, the encoder won't multiply it by 6.\n\n" +
    `version\\${version}\n` +
    `itemCount\\${itemCount}\n\n`;

let body = "";
for (const it of items) {
    body += "add_item\\" + presentFields.map(f => it[f]).join("\\") + "\n";
}

writeFileSync(outPath, header + body);
console.error(`wrote ${outPath}`);
