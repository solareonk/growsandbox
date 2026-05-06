// Extract Tile feature for GTools — Hybrid (manual + auto by item ID).
// Companion to tile_image.js. Reads from #tile_image_canvas, writes to #extract_result_canvas.
// Uses addEventListener so it won't conflict with the existing onclick property handlers.

(function () {
  // --- DOM refs ---
  // For drawing source: use the ORIGINAL canvas (no grid lines overlaid).
  // tile_image_canvas has visual grid drawn on top — would leak into extracted PNG.
  // tile_image_canvas_original is the clean source kept by tile_image.js.
  const srcCanvas       = document.getElementById('tile_image_canvas_original');
  // For mouse hit-testing: still use the displayed canvas (where user clicks).
  const displayCanvas   = document.getElementById('tile_image_canvas');
  const tileWidthInput  = document.getElementById('tileWidth');
  const tileHeightInput = document.getElementById('tileHeight');

  // The new UI elements (added in image/index.html)
  // Mode A: separate "extract_drag_mode" toggle. Independent from "Using Mouse Event"
  // so user can use either or both. When this is ON, mousedown+drag+mouseup on the atlas
  // selects a rectangle of cells.
  const dragModeChk     = document.getElementById('extract_drag_mode');
  const dragStatus      = document.getElementById('extract_drag_status');
  const loadItemsBtn    = document.getElementById('load_items_txt');
  const itemsStatus     = document.getElementById('items_txt_status');
  const itemIdInput     = document.getElementById('extract_item_id');
  const extractByIdBtn  = document.getElementById('extract_by_id');
  const idInfo          = document.getElementById('extract_id_info');
  const resultCanvas    = document.getElementById('extract_result_canvas');
  const downloadBtn     = document.getElementById('download_extract');
  const clearBtn        = document.getElementById('clear_extract');
  const resultCtx       = resultCanvas.getContext('2d');

  // --- State ---
  // extractCells: array of {col, row}. Order preserved; bounding box used at render.
  let extractCells = [];
  // itemsMap: Map<id, {tx, ty, spread, name, texture}>
  let itemsMap = null;

  // --- Helpers ---
  function tileW() { return parseInt(tileWidthInput.value) || 32; }
  function tileH() { return parseInt(tileHeightInput.value) || 32; }

  function parseItemsTxt(text) {
    const map = new Map();
    const lines = text.split('\n');
    for (const raw of lines) {
      const line = raw.replace(/[\r\n]+$/g, '');
      if (!line.startsWith('add_item')) continue;
      const parts = line.split('\\');
      // Expected indices (0-based after split):
      // 0=add_item, 1=item_id, 2=editable_type, 3=item_category, 4=action_type,
      // 5=hit_sound_type, 6=name, 7=texture, 8=texture_hash, 9=item_kind, 10=val1,
      // 11=texture_x, 12=texture_y, 13=spread_type, ...
      if (parts.length < 14) continue;
      const id      = parseInt(parts[1]);
      const name    = parts[6];
      const texture = parts[7];
      const tx      = parseInt(parts[11]);
      const ty      = parseInt(parts[12]);
      const spread  = parseInt(parts[13]);
      if (Number.isNaN(id) || Number.isNaN(tx) || Number.isNaN(ty) || Number.isNaN(spread)) continue;
      map.set(id, { tx, ty, spread, name, texture });
    }
    return map;
  }

  function getClusterSize(spreadType) {
    // Confirmed empirically:
    //   spread_type 1 → single tile (Dough id 956 etc.)
    //   spread_type 2 → 8x6 autotile cluster (Dirt id 2, Cave Background id 14)
    // Other spread_types: not yet confirmed — fall back to 1x1 with warning flag.
    if (spreadType === 2) return { w: 8, h: 6, confirmed: true };
    if (spreadType === 0 || spreadType === 1) return { w: 1, h: 1, confirmed: true };
    return { w: 1, h: 1, confirmed: false };  // unknown spread_type
  }

  // Find items that have anchor inside the cluster bounds, excluding the target item.
  // Co-anchor items (e.g., "Dirt Seed" sharing exact coord with "Dirt") are NOT borrowed —
  // Growtopia's convention pairs item+seed at the same texture_x/y, but the cell visually
  // belongs to the parent's autotile cluster.
  function findBorrowedCells(targetId, item, clusterW, clusterH) {
    const borrowed = [];
    if (!itemsMap) return borrowed;
    for (const [id, other] of itemsMap) {
      if (id === targetId) continue;
      // Same atlas only — items pointing to a different texture aren't relevant.
      if (other.texture !== item.texture) continue;
      // Skip co-anchor items (target's own seed/paired item).
      if (other.tx === item.tx && other.ty === item.ty) continue;
      if (other.tx >= item.tx && other.tx < item.tx + clusterW &&
          other.ty >= item.ty && other.ty < item.ty + clusterH) {
        borrowed.push({ col: other.tx, row: other.ty, id, name: other.name });
      }
    }
    return borrowed;
  }

  // --- Render extracted cells onto resultCanvas ---
  function redrawResult() {
    if (extractCells.length === 0) {
      resultCanvas.width  = 1;
      resultCanvas.height = 1;
      resultCtx.clearRect(0, 0, 1, 1);
      return;
    }

    let minC = Infinity, maxC = -Infinity, minR = Infinity, maxR = -Infinity;
    for (const { col, row } of extractCells) {
      if (col < minC) minC = col;
      if (col > maxC) maxC = col;
      if (row < minR) minR = row;
      if (row > maxR) maxR = row;
    }
    const cols = maxC - minC + 1;
    const rows = maxR - minR + 1;
    const tw = tileW();
    const th = tileH();

    resultCanvas.width  = cols * tw;
    resultCanvas.height = rows * th;
    resultCtx.imageSmoothingEnabled = false;
    resultCtx.clearRect(0, 0, resultCanvas.width, resultCanvas.height);

    for (const { col, row } of extractCells) {
      const sx = col * tw;
      const sy = row * th;
      const dx = (col - minC) * tw;
      const dy = (row - minR) * th;
      resultCtx.drawImage(srcCanvas, sx, sy, tw, th, dx, dy, tw, th);
    }
  }

  // --- Mode A: drag-to-grab rectangle of cells ---
  // mousedown records start cell, mousemove shows live preview (existing extractCells +
  // current drag rect), mouseup commits the rect to extractCells.
  // Single click (mousedown+mouseup at same cell) adds just that 1 cell.
  let dragging   = false;
  let dragStartC = -1, dragStartR = -1;
  let dragCurrC  = -1, dragCurrR  = -1;

  function eventToCell(event) {
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const px = (event.clientX - rect.left) * scaleX;
    const py = (event.clientY - rect.top) * scaleY;
    return {
      col: Math.floor(px / tileW()),
      row: Math.floor(py / tileH()),
    };
  }

  // Render extractCells PLUS the live drag rectangle (if dragging)
  function redrawResultWithDrag() {
    // Compute combined cell set: extractCells + current drag rect
    const dragCells = [];
    if (dragging && dragStartC >= 0) {
      const c0 = Math.min(dragStartC, dragCurrC);
      const c1 = Math.max(dragStartC, dragCurrC);
      const r0 = Math.min(dragStartR, dragCurrR);
      const r1 = Math.max(dragStartR, dragCurrR);
      for (let r = r0; r <= r1; r++) {
        for (let c = c0; c <= c1; c++) dragCells.push({ col: c, row: r });
      }
    }

    // Combine, deduplicate
    const all = [...extractCells];
    for (const dc of dragCells) {
      if (!all.some(c => c.col === dc.col && c.row === dc.row)) all.push(dc);
    }

    if (all.length === 0) {
      resultCanvas.width = 1; resultCanvas.height = 1;
      resultCtx.clearRect(0, 0, 1, 1);
      return;
    }

    let minC = Infinity, maxC = -Infinity, minR = Infinity, maxR = -Infinity;
    for (const { col, row } of all) {
      if (col < minC) minC = col;
      if (col > maxC) maxC = col;
      if (row < minR) minR = row;
      if (row > maxR) maxR = row;
    }
    const cols = maxC - minC + 1;
    const rows = maxR - minR + 1;
    const tw = tileW(), th = tileH();

    resultCanvas.width  = cols * tw;
    resultCanvas.height = rows * th;
    resultCtx.imageSmoothingEnabled = false;
    resultCtx.clearRect(0, 0, resultCanvas.width, resultCanvas.height);

    for (const { col, row } of all) {
      resultCtx.drawImage(srcCanvas,
        col * tw, row * th, tw, th,
        (col - minC) * tw, (row - minR) * th, tw, th);
    }

    // Tint the in-progress drag cells subtly so user sees they're tentative
    if (dragging) {
      resultCtx.fillStyle = 'rgba(255, 220, 0, 0.25)';
      for (const { col, row } of dragCells) {
        resultCtx.fillRect((col - minC) * tw, (row - minR) * th, tw, th);
      }
    }
  }

  function modeAActive() {
    return !!(dragModeChk && dragModeChk.checked);
  }

  // Capture-phase listeners: fire BEFORE tile_image.js's onclick/onmousedown/onmouseup
  // property handlers, so we can call stopImmediatePropagation to prevent the Edit-Tile
  // modal from opening when Mode A is active.
  displayCanvas.addEventListener('mousedown', function (event) {
    if (!modeAActive()) return;
    if (event.button !== 0) return;  // left click only
    event.stopImmediatePropagation();
    event.preventDefault();
    dragging = true;
    const cell = eventToCell(event);
    dragStartC = cell.col; dragStartR = cell.row;
    dragCurrC  = cell.col; dragCurrR  = cell.row;
    if (dragStatus) dragStatus.innerText = `Dragging from (${dragStartC}, ${dragStartR})…`;
    redrawResultWithDrag();
  }, true);

  displayCanvas.addEventListener('mousemove', function (event) {
    if (!modeAActive()) return;
    if (!dragging) return;
    event.stopImmediatePropagation();
    const cell = eventToCell(event);
    if (cell.col === dragCurrC && cell.row === dragCurrR) return;  // no change
    dragCurrC = cell.col; dragCurrR = cell.row;
    const c0 = Math.min(dragStartC, dragCurrC);
    const c1 = Math.max(dragStartC, dragCurrC);
    const r0 = Math.min(dragStartR, dragCurrR);
    const r1 = Math.max(dragStartR, dragCurrR);
    if (dragStatus) {
      dragStatus.innerText = `Drag: (${c0}, ${r0}) → (${c1}, ${r1}) = ${(c1-c0+1)*(r1-r0+1)} cell(s)`;
    }
    redrawResultWithDrag();
  }, true);

  function commitDrag() {
    if (!dragging) return;
    const c0 = Math.min(dragStartC, dragCurrC);
    const c1 = Math.max(dragStartC, dragCurrC);
    const r0 = Math.min(dragStartR, dragCurrR);
    const r1 = Math.max(dragStartR, dragCurrR);
    let added = 0;
    for (let r = r0; r <= r1; r++) {
      for (let c = c0; c <= c1; c++) {
        if (!extractCells.some(x => x.col === c && x.row === r)) {
          extractCells.push({ col: c, row: r });
          added++;
        }
      }
    }
    dragging = false;
    dragStartC = dragStartR = dragCurrC = dragCurrR = -1;
    if (dragStatus) {
      dragStatus.innerText = `Added ${added} new cell(s). Total: ${extractCells.length}.`;
    }
    redrawResultWithDrag();
  }

  displayCanvas.addEventListener('mouseup', function (event) {
    if (!modeAActive()) return;
    event.stopImmediatePropagation();
    commitDrag();
  }, true);

  // Also commit + block click event when Mode A active (so the existing onclick handler
  // — which opens modal when using_mouse_event is OFF — doesn't fire).
  displayCanvas.addEventListener('click', function (event) {
    if (!modeAActive()) return;
    event.stopImmediatePropagation();
    event.preventDefault();
  }, true);

  // Mouse leaves canvas while dragging — avoid stuck-drag state
  displayCanvas.addEventListener('mouseleave', function () {
    if (dragging) commitDrag();
  });

  // --- Mode B: load items.txt ---
  loadItemsBtn.addEventListener('click', function () {
    const fileInput = document.createElement('input');
    fileInput.type = 'file';
    fileInput.accept = '.txt';
    fileInput.style.display = 'none';
    fileInput.onchange = function (e) {
      const file = e.target.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = function (ev) {
        itemsMap = parseItemsTxt(ev.target.result);
        itemsStatus.innerText = `Loaded ${itemsMap.size} items from ${file.name}`;
      };
      reader.readAsText(file);
    };
    document.body.appendChild(fileInput);
    fileInput.click();
    document.body.removeChild(fileInput);
  });

  // --- Mode B: extract by item ID ---
  extractByIdBtn.addEventListener('click', function () {
    if (!itemsMap) {
      idInfo.innerText = 'Load items.txt first.';
      return;
    }
    const id = parseInt(itemIdInput.value);
    if (Number.isNaN(id) || !itemsMap.has(id)) {
      idInfo.innerText = `Item id ${itemIdInput.value} not found.`;
      return;
    }
    const item = itemsMap.get(id);
    const cluster = getClusterSize(item.spread);
    const borrowed = findBorrowedCells(id, item, cluster.w, cluster.h);

    extractCells = [];
    for (let r = item.ty; r < item.ty + cluster.h; r++) {
      for (let c = item.tx; c < item.tx + cluster.w; c++) {
        const isBorrowed = borrowed.some(b => b.col === c && b.row === r);
        if (!isBorrowed) extractCells.push({ col: c, row: r });
      }
    }

    redrawResult();

    let msg = `Item ${id} "${item.name}": atlas ${item.texture}, anchor (${item.tx}, ${item.ty}), `
            + `spread_type=${item.spread}, cluster ${cluster.w}x${cluster.h}. `
            + `Extracted ${extractCells.length} cells.`;
    if (!cluster.confirmed) msg += ' [WARN: spread_type unknown — falls back to 1x1.]';
    if (borrowed.length > 0) {
      const list = borrowed.map(b => `id ${b.id} "${b.name}" at (${b.col}, ${b.row})`).join('; ');
      msg += ` Borrowed cells excluded: ${list}.`;
    }
    idInfo.innerText = msg;
  });

  // --- Download ---
  downloadBtn.addEventListener('click', function () {
    if (extractCells.length === 0) {
      idInfo.innerText = 'Nothing to extract.';
      return;
    }
    const link = document.createElement('a');
    link.download = 'tiles_extracted.png';
    link.href = resultCanvas.toDataURL('image/png');
    link.click();
  });

  // --- Clear ---
  clearBtn.addEventListener('click', function () {
    extractCells = [];
    redrawResult();
    if (idInfo) idInfo.innerText = 'Cleared.';
  });
})();
