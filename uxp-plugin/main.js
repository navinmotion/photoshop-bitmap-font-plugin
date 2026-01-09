// --- LOGGING ---
function logStep(msg) {
    const con = document.getElementById('uiConsole');
    if (con) {
        con.innerText += `> ${msg}\n`;
        con.scrollTop = con.scrollHeight;
    }
    console.log(msg);
}

window.onerror = function (msg, url, line) {
    logStep(`JS Err: ${msg} (Line ${line})`);
    return false;
};

// --- HELPERS ---
const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';

function safeBase64ToArrayBuffer(input) {
    if (!input) return new ArrayBuffer(0);
    const str = input.replace(/=+$/, '');
    if (str.length % 4 == 1) throw new Error("Invalid base64");
    let output = '';
    for (let bc = 0, bs = 0, buffer, i = 0; buffer = str.charAt(i++); ~buffer && (bs = bc % 4 ? bs * 64 + buffer : buffer, bc++ % 4) ? output += String.fromCharCode(255 & bs >> (-2 * bc & 6)) : 0) {
        buffer = chars.indexOf(buffer);
    }
    const len = output.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) bytes[i] = output.charCodeAt(i);
    return bytes.buffer;
}

// Sleep helper
const sleep = ms => new Promise(r => setTimeout(r, ms));

// --- STATE ---
const WS_URL = 'ws://localhost:4567';
let socket = null;
let isConnected = false;
let currentFntData = "";
let currentZoom = 1.0;

// --- UI INIT ---
const els = {};
function initUI() {
    const ids = ['connectionStatus', 'previewImage', 'textInput', 'fontFamily', 'btnLoadFont', 'customFontPath', 'loadedFontName', 'fontSize', 'sizingMethod', 'fixedSizeGroup', 'atlasWidth', 'padding', 'spacing', 'effectPadding', 'fileName', 'btnExport', 'btnSendToPS', 'globalXAdvance', 'globalXOffset', 'globalYOffset'];
    ids.forEach(id => els[id] = document.getElementById(id));

    if (els.spacing) els.spacing.value = "0";

    els.btnExport.addEventListener('click', exportFiles);
    els.btnSendToPS.addEventListener('click', sendToPhotoshop);
    els.btnLoadFont.addEventListener('click', loadCustomFont);
    els.sizingMethod.addEventListener('change', () => { updateUiState(); requestGen(); });
    els.fontFamily.addEventListener('change', requestGen);

    // Zoom Listener
    const prevWrap = document.querySelector('.preview-wrapper');
    if (prevWrap) {
        prevWrap.addEventListener('wheel', (e) => {
            if (e.ctrlKey || e.metaKey || e.deltaY) {
                e.preventDefault();
                const delta = e.deltaY > 0 ? -0.1 : 0.1;
                updateZoom(delta);
            }
        }, { passive: false });
    }

    // Input Listeners
    ['textInput', 'fontSize', 'atlasWidth', 'padding', 'spacing', 'effectPadding', 'globalXAdvance', 'globalXOffset', 'globalYOffset'].forEach(k => {
        if (els[k]) els[k].addEventListener('input', debounce(requestGen, 300));
    });

    updateUiState();
}

function updateUiState() {
    const method = els.sizingMethod.value;
    els.fixedSizeGroup.style.display = (method === 'fixed') ? 'flex' : 'none';
}

function updateZoom(delta) {
    if (!els.previewImage || !els.previewImage.src) return;

    let newZoom = currentZoom + delta;
    if (newZoom < 0.1) newZoom = 0.1;
    if (newZoom > 8.0) newZoom = 8.0;

    currentZoom = newZoom;

    if (els.previewImage.naturalWidth) {
        els.previewImage.style.transform = "none";
        els.previewImage.style.width = `${els.previewImage.naturalWidth * currentZoom}px`;
        els.previewImage.style.height = "auto";
    }
}

let debounceTimer;
function debounce(func, delay) {
    return function () {
        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => func.apply(this, arguments), delay);
    }
}

// --- FONT ---
async function loadCustomFont() {
    if (typeof require === "undefined") return;
    const fs = require('uxp').storage.localFileSystem;
    const files = await fs.getFileForOpening({ types: ['ttf', 'otf', 'TTF', 'OTF'], allowMultiple: false });
    if (!files) return;
    const entry = Array.isArray(files) ? files[0] : files;
    if (!entry) return;

    els.customFontPath.value = entry.nativePath;
    let opt = els.fontFamily.querySelector('option[value="custom_loaded"]');
    if (!opt) {
        opt = document.createElement('option');
        opt.value = "custom_loaded";
        opt.disabled = false;
        els.fontFamily.add(opt, 0);
    }
    opt.text = `File: ${entry.name}`;
    els.fontFamily.value = "custom_loaded";
    els.loadedFontName.style.display = 'block';
    els.loadedFontName.innerText = entry.nativePath;
    requestGen();
}

// --- ENGINE ---
function connect() {
    try {
        socket = new WebSocket(WS_URL);
        socket.onopen = () => {
            isConnected = true;
            els.connectionStatus.innerHTML = '<span class="status-dot"></span> Online';
            els.connectionStatus.className = "status on";
            logStep("Engine Connected.");
            requestGen();
        };
        socket.onclose = () => {
            isConnected = false;
            els.connectionStatus.innerHTML = '<span class="status-dot"></span> Offline';
            els.connectionStatus.className = "status off";
        };
        socket.onmessage = (e) => {
            try {
                const data = JSON.parse(e.data);
                if (data.image) {
                    els.previewImage.src = "data:image/png;base64," + data.image;
                    // Refresh dimensions if zoomed
                    if (els.previewImage.style.width) {
                        els.previewImage.style.width = `${(els.previewImage.naturalWidth || 512) * currentZoom}px`;
                    }
                }
                if (data.fnt) currentFntData = data.fnt;
            } catch (err) { console.error(err); }
        };
    } catch (e) { logStep("Socket Error"); }
}

function requestGen() {
    if (!isConnected) return;
    const payload = {
        text: els.textInput.value || " ",
        font: (els.fontFamily.value === "custom_loaded" ? els.customFontPath.value : els.fontFamily.value),
        size: parseInt(els.fontSize.value) || 32,
        width: parseInt(els.atlasWidth.value) || 512,
        padding: parseInt(els.padding.value) || 0,
        spacing: parseInt(els.spacing.value) || 0,
        effectPadding: parseInt(els.effectPadding.value) || 0,
        autoPack: els.sizingMethod.value !== 'fixed',
        packMode: els.sizingMethod.value === 'auto_m4' ? 'mult4' : 'pot',
        globalXAdvance: parseInt(els.globalXAdvance.value) || 0,
        globalXOffset: parseInt(els.globalXOffset.value) || 0,
        globalYOffset: parseInt(els.globalYOffset.value) || 0
    };
    socket.send(JSON.stringify(payload));
}

// --- EXPORT ---
async function exportFiles() {
    if (typeof require === "undefined") return;
    if (!els.previewImage.src) return;
    try {
        const fs = require('uxp').storage.localFileSystem;
        const formats = require('uxp').storage.formats;
        const baseName = els.fileName.value.trim() || "atlas";
        const folder = await fs.getFolder();
        if (!folder) return;

        const parts = els.previewImage.src.split(',');
        const arrayBuffer = safeBase64ToArrayBuffer(parts[1]);
        const pngFile = await folder.createFile(`${baseName}.png`, { overwrite: true });
        await pngFile.write(arrayBuffer, { format: formats.binary });

        if (currentFntData) {
            let finalFnt = currentFntData.replace(/file=".*?"/, `file="${baseName}.png"`);
            const fntFile = await folder.createFile(`${baseName}.fnt`, { overwrite: true });
            await fntFile.write(finalFnt, { format: formats.utf8 });
        }
        logStep("Files Exported.");
    } catch (e) { logStep("Export Error: " + e.message); }
}

// --- PHOTOSHOP LOGIC (WIPE & REWRITE) ---
async function sendToPhotoshop() {
    if (typeof require === "undefined") return;

    // 1. Validation
    if (!els.previewImage.src) { logStep("Error: No preview image found."); return; }
    if (!currentFntData) { logStep("Error: No FNT metadata found."); return; }

    const parts = els.previewImage.src.split(',');
    if (parts.length < 2) { logStep("Error: Invalid Image Data."); return; }

    // 2. Prep Data
    const arrayBuffer = safeBase64ToArrayBuffer(parts[1]);
    const fs = require('uxp').storage.localFileSystem;
    const formats = require('uxp').storage.formats;
    const batchPlay = require('photoshop').action.batchPlay;
    const executeAsModal = require('photoshop').core.executeAsModal;
    const app = require('photoshop').app;

    const w = els.previewImage.naturalWidth || 512;
    const h = els.previewImage.naturalHeight || 512;
    const fileName = els.fileName.value || "Atlas";
    const glyphs = parseFnt(currentFntData);

    try {
        await executeAsModal(async () => {
            logStep("Phase 1: Setup...");

            let targetDoc = app.activeDocument;
            
            // --- A. DOCUMENT SETUP (Create or Wipe) ---
            if (targetDoc) {
                logStep("Updating & Wiping Doc...");
                
                // 1. Resize Canvas
                await batchPlay([{
                    "_obj": "imageSize",
                    "width": { "_unit": "pixelsUnit", "_value": w },
                    "height": { "_unit": "pixelsUnit", "_value": h },
                    "scaleStyles": true
                }], { modalBehavior: "execute" });

                // 2. CLEANUP: Delete ALL existing layers
                // Strategy: Create a "Temp_Safe" layer, Select All, Deselect Temp_Safe, Delete Selection.
                
                // Create Temp Safe Layer (so doc is never empty)
                await batchPlay([
                    { "_obj": "make", "_target": [{ "_ref": "layer" }] },
                    { "_obj": "set", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }], "to": { "_obj": "layer", "name": "Temp_Safe_Layer" } }
                ], { modalBehavior: "execute" });

                // Select All Layers
                await batchPlay([{ "_obj": "selectAllLayers", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }] }], { modalBehavior: "execute" });

                // Deselect the Temp Safe Layer
                await batchPlay([{ "_obj": "select", "_target": [{ "_ref": "layer", "_name": "Temp_Safe_Layer" }], "selectionModifier": { "_enum": "selectionModifierType", "_value": "removeFromSelection" } }], { modalBehavior: "execute" });

                // Delete everything else (The Wipe)
                try {
                    await batchPlay([{ "_obj": "delete", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }] }], { modalBehavior: "execute" });
                } catch(e) { /* Ignore if selection was empty (i.e. nothing to delete) */ }

            } else {
                logStep("Creating New Doc...");
                await batchPlay([{
                    "_obj": "make",
                    "new": {
                        "_obj": "document",
                        "width": { "_unit": "pixelsUnit", "_value": w },
                        "height": { "_unit": "pixelsUnit", "_value": h },
                        "resolution": { "_unit": "densityUnit", "_value": 72 },
                        "mode": { "_class": "RGBColorMode" },
                        "fill": { "_enum": "fill", "_value": "transparent" },
                        "name": fileName
                    }
                }], { modalBehavior: "execute" });
                
                // Even a new doc might have "Layer 1" or "Background". Clean it for consistency.
                // We just rely on Placing the Atlas next to cover it, but let's be clean.
                // (Skip strict wipe on new doc to save time, assuming standard transparent start is fine).
            }

            // --- B. PLACE ATLAS ---
            logStep("Placing Atlas...");
            const tempFolder = await fs.getTemporaryFolder();
            const file = await tempFolder.createFile("temp_atlas_slice.png", { overwrite: true });
            await file.write(arrayBuffer, { format: formats.binary });
            const token = await fs.createSessionToken(file);

            await batchPlay([
                { "_obj": "placeEvent", "ID": 1, "null": { "_path": token, "_kind": "local" }, "freeTransformCenterState": { "_enum": "quadCenterState", "_value": "QCSAverage" }, "offset": { "_obj": "offset", "horizontal": { "_unit": "pixelsUnit", "_value": 0 }, "vertical": { "_unit": "pixelsUnit", "_value": 0 } } },
                { "_obj": "rasterizeLayer", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }] },
                { "_obj": "set", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }], "to": { "_obj": "layer", "name": "AtlasSource" } }
            ], { modalBehavior: "execute" });

            // --- C. CLEANUP TEMP LAYER ---
            // If we created "Temp_Safe_Layer" during wipe, delete it now that we have AtlasSource.
            try {
                await batchPlay([
                    { "_obj": "select", "_target": [{ "_ref": "layer", "_name": "Temp_Safe_Layer" }] },
                    { "_obj": "delete", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }] }
                ], { modalBehavior: "execute" });
            } catch(e) { /* Layer might not exist if it was a New Doc flow */ }

            // --- D. SLICE GLYPHS ---
            logStep(`Slicing ${glyphs.length} glyphs...`);
            const chunkSize = 30; 
            for (let i = 0; i < glyphs.length; i += chunkSize) {
                const chunk = glyphs.slice(i, i + chunkSize);
                const cmds = [];
                chunk.forEach(g => {
                    if (g.w <= 0 || g.h <= 0) return;
                    cmds.push({ "_obj": "select", "_target": [{ "_ref": "layer", "_name": "AtlasSource" }], "makeVisible": true });
                    cmds.push({ "_obj": "set", "_target": [{ "_ref": "channel", "_property": "selection" }], "to": { "_obj": "rectangle", "top": { "_unit": "pixelsUnit", "_value": g.y }, "left": { "_unit": "pixelsUnit", "_value": g.x }, "bottom": { "_unit": "pixelsUnit", "_value": g.y + g.h }, "right": { "_unit": "pixelsUnit", "_value": g.x + g.w } } });
                    cmds.push({ "_obj": "copyToLayer" });
                    cmds.push({ "_obj": "set", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }], "to": { "_obj": "layer", "name": `Glyph_${g.id}` } });
                });
                if(cmds.length) await batchPlay(cmds, { modalBehavior: "execute" });
            }

            // --- E. FINALIZE ---
            logStep("Finalizing...");
            
            // Delete AtlasSource
            await batchPlay([
                { "_obj": "select", "_target": [{ "_ref": "layer", "_name": "AtlasSource" }] },
                { "_obj": "delete", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }] },
                { "_obj": "set", "_target": [{ "_ref": "channel", "_property": "selection" }], "to": { "_enum": "ordinal", "_value": "none" } }
            ], { modalBehavior: "execute" });

            // Group Glyphs
            // We group EVERYTHING remaining (which should just be the glyphs)
            await batchPlay([
                { "_obj": "selectAllLayers", "_target": [{ "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }] },
                { "_obj": "make", "new": { "_obj": "layerSection" }, "from": { "_ref": "layer", "_enum": "ordinal", "_value": "targetEnum" }, "name": "Atlas_Glyphs" }
            ], { modalBehavior: "execute" });

        }, { commandName: "Atlas Gen: Direct Slice" });

        // Update Button Text
        els.btnSendToPS.innerText = "Update Doc";
        logStep("Done! Check 'Atlas_Glyphs' folder.");

    } catch (e) {
        let msg = typeof e === "string" ? e : (e.message || JSON.stringify(e));
        logStep("PS Error: " + msg);
        console.error(e);
    }
}

function parseFnt(fntText) {
    const glyphs = [];
    if (!fntText) return glyphs;
    const lines = fntText.split('\n');
    const regex = /char id=(\d+)\s+x=(\d+)\s+y=(\d+)\s+width=(\d+)\s+height=(\d+)/;
    for (let line of lines) {
        if (line.startsWith("char")) {
            const m = line.match(regex);
            if (m) {
                glyphs.push({
                    id: parseInt(m[1]), x: parseInt(m[2]), y: parseInt(m[3]), w: parseInt(m[4]), h: parseInt(m[5])
                });
            }
        }
    }
    return glyphs;
}

document.addEventListener('DOMContentLoaded', () => { initUI(); connect(); });