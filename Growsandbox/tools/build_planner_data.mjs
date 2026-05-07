// Build planner_data.json from decoded items.txt.
// Filter: only items whose texture is a tiles_page* or water (placeable on world grid).
//
// Usage: node build_planner_data.mjs <items.txt> <planner_data.json>

import { readFileSync, writeFileSync } from "node:fs";

const [, , inPath, outPath] = process.argv;
if (!inPath || !outPath) {
    console.error("Usage: node build_planner_data.mjs <items.txt> <planner_data.json>");
    process.exit(1);
}

const FIELD = {
    item_id: 1, editable_type: 2, item_category: 3, action_type: 4, hit_sound_type: 5,
    name: 6, texture: 7, texture_hash: 8, item_kind: 9, val1: 10,
    texture_x: 11, texture_y: 12, spread_type: 13, is_stripey_wallpaper: 14,
    collision_type: 15, break_hits: 16, drop_chance: 17, clothing_type: 18,
    rarity: 19, max_amount: 20,
};

const text = readFileSync(inPath, "utf-8");
const lines = text.split("\n");

const data = {};
let included = 0, skipped = 0;
const fileHistogram = {};
const spreadHistogram = {};

for (const line of lines) {
    if (!line.startsWith("add_item\\")) continue;
    const f = line.split("\\");
    const name = f[FIELD.name];
    const texture = f[FIELD.texture];
    if (!texture) { skipped++; continue; }

    // Only world-placeable tiles
    const isTilePage = /^tiles_page(\d+)(_winter)?\.rttex$/i.test(texture);
    const isWater = /^water(_ray)?\.rttex$/i.test(texture);
    if (!isTilePage && !isWater) { skipped++; continue; }

    // Skip seeds — share spritesheet with their parent block but aren't
    // placeable as world tiles. 99.99% have action_type=19; one outlier
    // uses action_type=37 (caught by the name suffix check).
    const actionType = parseInt(f[FIELD.action_type], 10);
    if (actionType === 19 || name.endsWith(" Seed")) { skipped++; continue; }

    // Skip duplicates (later definition wins is unusual; first wins is fine)
    if (data[name]) { skipped++; continue; }

    const file = texture.replace(/\.rttex$/i, ".png");
    const spread = parseInt(f[FIELD.spread_type], 10) || 1;
    const x = parseInt(f[FIELD.texture_x], 10) || 0;
    const y = parseInt(f[FIELD.texture_y], 10) || 0;

    data[name] = {
        spread,
        file,
        x,
        y,
        item_id: parseInt(f[FIELD.item_id], 10),
        item_kind: parseInt(f[FIELD.item_kind], 10),
        action_type: parseInt(f[FIELD.action_type], 10),
        item_category: parseInt(f[FIELD.item_category], 10),
        collision_type: parseInt(f[FIELD.collision_type], 10),
    };
    included++;
    fileHistogram[file] = (fileHistogram[file] || 0) + 1;
    spreadHistogram[spread] = (spreadHistogram[spread] || 0) + 1;
}

writeFileSync(outPath, JSON.stringify(data, null, 2));

console.error(`included: ${included}, skipped: ${skipped}`);
console.error(`per-file:`);
for (const [f, n] of Object.entries(fileHistogram).sort()) console.error(`  ${f}: ${n}`);
console.error(`per-spread:`);
for (const [s, n] of Object.entries(spreadHistogram).sort((a, b) => +a[0] - +b[0])) {
    console.error(`  spread ${s}: ${n}`);
}
console.error(`wrote ${outPath}`);
